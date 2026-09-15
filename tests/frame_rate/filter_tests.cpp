// SPDX-License-Identifier: GPL-2.0-or-later
// Runtime differential tests: no link dependency on AvsCore or its private headers.
#define NOMINMAX
#include <avisynth.h>
#include "../common/reference_clip.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

const AVS_Linkage* AVS_linkage = nullptr;

namespace {
int cases = 0;
void require(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}
struct Runtime {
#if defined(_WIN32)
  HMODULE module;
  explicit Runtime(const char* path) : module(LoadLibraryA(path)) {}
  void* symbol(const char* name) { return reinterpret_cast<void*>(GetProcAddress(module, name)); }
  ~Runtime() {
    if (module)
      FreeLibrary(module);
  }
#else
  void* module;
  explicit Runtime(const char* path) : module(dlopen(path, RTLD_NOW | RTLD_LOCAL)) {}
  void* symbol(const char* name) { return dlsym(module, name); }
  ~Runtime() {
    if (module)
      dlclose(module);
  }
#endif
};
struct Environment {
  IScriptEnvironment2* env;
  explicit Environment(Runtime& runtime) {
    using Create = IScriptEnvironment2*(__stdcall*)(int);
    auto create = reinterpret_cast<Create>(runtime.symbol("CreateScriptEnvironment2"));
    require(create != nullptr, "missing CreateScriptEnvironment2");
    env = create(AVISYNTH_INTERFACE_VERSION);
    require(env != nullptr, "cannot create matching runtime");
    AVS_linkage = env->GetAVSLinkage();
  }
  ~Environment() { env->DeleteScriptEnvironment(); }
};

std::vector<int> planes(const VideoInfo& vi) {
  if (!vi.IsPlanar())
    return {0};
  std::vector<int> p = vi.IsPlanarRGB() || vi.IsPlanarRGBA() ? std::vector<int>{PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A}
                                                             : std::vector<int>{PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
  p.resize(vi.NumComponents());
  return p;
}

class Sequence final : public IClip {
  VideoInfo vi_{};
  std::vector<PVideoFrame> frames_;

public:
  Sequence(IScriptEnvironment* env, int type, int width, int height, bool stable) {
    vi_.width = width;
    vi_.height = height;
    vi_.pixel_type = type;
    vi_.num_frames = 5;
    vi_.fps_numerator = 25;
    vi_.fps_denominator = 1;
    vi_.audio_samples_per_second = 48000;
    vi_.nchannels = 2;
    vi_.sample_type = SAMPLE_INT16;
    vi_.num_audio_samples = 9600;
    const int size = vi_.ComponentSize();
    for (int n = 0; n < vi_.num_frames; ++n) {
      PVideoFrame f = env->NewVideoFrame(vi_);
      for (int p : planes(vi_)) {
        uint8_t* data = f->GetWritePtr(p);
        // Initialize host padding as well to catch accidental active dependence.
        std::memset(data, 0x71 + n, f->GetPitch(p) * f->GetHeight(p));
        for (int y = 0; y < f->GetHeight(p); ++y)
          for (int x = 0; x < f->GetRowSize(p) / size; ++x) {
            const unsigned value = (x * 37 + y * 13 + p * 3 + n * (stable ? 3 : 61)) & 255;
            uint8_t* row = data + y * f->GetPitch(p);
            if (size == 1)
              row[x] = uint8_t(value);
            else if (size == 2)
              reinterpret_cast<uint16_t*>(row)[x] = uint16_t((value * 257 + x) & ((1u << vi_.BitsPerComponent()) - 1));
            else
              reinterpret_cast<float*>(row)[x] = float(value) / 191 - .25f;
          }
      }
      env->propSetInt(env->getFramePropsRW(f), "AIFTest", 700 + n, PROPAPPENDMODE_REPLACE);
      frames_.push_back(f);
    }
  }
  int __stdcall GetVersion() override { return AVISYNTH_INTERFACE_VERSION; }
  const VideoInfo& __stdcall GetVideoInfo() override { return vi_; }
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment*) override {
    return frames_.at(std::clamp(n, 0, vi_.num_frames - 1));
  }
  bool __stdcall GetParity(int n) override { return (n & 1) != 0; }
  void __stdcall GetAudio(void* buf, int64_t start, int64_t count, IScriptEnvironment*) override {
    auto* samples = static_cast<int16_t*>(buf);
    for (int64_t i = 0; i < count * 2; ++i)
      samples[i] = int16_t((start + i) % 123);
  }
  int __stdcall SetCacheHints(int, int) override { return 0; }
};

std::vector<uint8_t> snapshot(PVideoFrame frame, const VideoInfo& vi) {
  std::vector<uint8_t> result;
  for (int p : planes(vi))
    for (int y = 0; y < frame->GetHeight(p); ++y) {
      const uint8_t* row = frame->GetReadPtr(p) + y * frame->GetPitch(p);
      result.insert(result.end(), row, row + frame->GetRowSize(p));
    }
  return result;
}

void compare(IScriptEnvironment* reference_env, IScriptEnvironment* env, const char* name,
             const std::vector<AVSValue>& args) {
  ++cases;
  const std::string migrated = std::string(name);
  PClip source = args[0].AsClip();
  const auto& vi = source->GetVideoInfo();
  std::vector<std::vector<uint8_t>> before;
  for (int n = 0; n < vi.num_frames; ++n)
    before.push_back(snapshot(source->GetFrame(n, env), vi));
  auto reference_args = reference_arguments(args, env, reference_env);
  for (auto& arg : reference_args)
    if (arg.IsClip())
      arg = reference_env->Invoke("Cache", arg);
  PClip old_filter = reference_env->Invoke(name, AVSValue(reference_args.data(), int(reference_args.size()))).AsClip();
  PClip new_filter = env->Invoke(migrated.c_str(), AVSValue(args.data(), int(args.size()))).AsClip();
  const auto& out = new_filter->GetVideoInfo();
  const auto& ref = old_filter->GetVideoInfo();
  require(out.width == ref.width && out.height == ref.height && out.pixel_type == ref.pixel_type &&
              out.num_frames == ref.num_frames && out.num_audio_samples == ref.num_audio_samples &&
              out.fps_numerator == ref.fps_numerator && out.fps_denominator == ref.fps_denominator &&
              out.audio_samples_per_second == ref.audio_samples_per_second,
          "metadata changed");
  for (int n : {out.num_frames - 1, 0, out.num_frames / 2, out.num_frames / 2}) {
    PVideoFrame a = old_filter->GetFrame(n, reference_env), b = new_filter->GetFrame(n, env);
    const auto aa = snapshot(a, ref), bb = snapshot(b, out);
    bool equal = aa == bb;
    if (!equal) {
      size_t first = 0;
      while (first < aa.size() && aa[first] == bb[first])
        ++first;
      std::fprintf(stderr, "%s mismatch: type=%d width=%d frame=%d byte=%zu (%u/%u)\n", name, vi.pixel_type, vi.width,
                   n, first, unsigned(aa[first]), unsigned(bb[first]));
      throw std::runtime_error("migrated output differs from built-in");
    }
    int error = 0;
    int ref_error = 0;
    const auto prop = reference_env->propGetInt(reference_env->getFramePropsRO(a), "AIFTest", 0, &ref_error);
    require(env->propGetInt(env->getFramePropsRO(b), "AIFTest", 0, &error) == prop && error == ref_error,
            "frame properties lost");
    require(new_filter->GetParity(n) == old_filter->GetParity(n), "parity changed");
  }
  int16_t old_audio[32], new_audio[32];
  old_filter->GetAudio(old_audio, 31, 16, reference_env);
  new_filter->GetAudio(new_audio, 31, 16, env);
  require(std::memcmp(old_audio, new_audio, sizeof(old_audio)) == 0, "audio passthrough changed");
  for (int n = 0; n < vi.num_frames; ++n)
    require(snapshot(source->GetFrame(n, env), vi) == before[n], "source frame modified");
}

class RateClip final : public GenericVideoFilter {
public:
  RateClip(PClip clip, unsigned num, unsigned den, int frames = 0) : GenericVideoFilter(clip) {
    if (frames)
      vi.num_frames = frames;
    vi.fps_numerator = num;
    vi.fps_denominator = den;
  }
};
void check_rate_limits(IScriptEnvironment* env) {
  PClip source(new Sequence(env, VideoInfo::CS_Y8, 32, 8, false));
  auto reject = [&](const char* name, std::vector<AVSValue> args, const char* message) {
    ++cases;
    try {
      env->Invoke(name, AVSValue(args.data(), int(args.size())));
    } catch (const AvisynthError& e) {
      require(std::strstr(e.msg, message) != nullptr, "unexpected boundary error");
      return;
    }
    throw std::runtime_error("unsafe FPS arithmetic accepted");
  };
  for (const char* name : {"AssumeFPS", "ChangeFPS", "ConvertFPS"}) {
    for (double value : {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN()})
      reject(name, {source, value}, "FPS");
    PClip same(new RateClip(source, 2147483647, 2147483646));
    AVSValue args[] = {same, 2147483647, 2147483646};
    PClip result = env->Invoke(name, AVSValue(args, 3)).AsClip();
    require(result->GetVideoInfo().num_frames == 5, "identity ratio changed frame count");
    require(snapshot(result->GetFrame(4, env), result->GetVideoInfo()) ==
                snapshot(source->GetFrame(4, env), source->GetVideoInfo()),
            "identity frame changed");
    ++cases;
  }
  for (const char* name : {"ChangeFPS", "ConvertFPS"}) {
    PClip different(new RateClip(source, 2147483647, 2147483646));
    reject(name, {different, 2147483645, 2147483644}, "Reduced FPS conversion factors");
  }
  // Largest supported reduced factors at a near-INT_MAX legal frame index.
  for (const char* name : {"ChangeFPS", "ConvertFPS"}) {
    PClip high(new RateClip(source, UINT32_MAX, 1, INT32_MAX));
    PClip target(new RateClip(source, UINT32_MAX - 1, 1));
    const AVSValue args[] = {high, target};
    PClip result = env->Invoke(name, AVSValue(args, 2)).AsClip();
    require(result->GetVideoInfo().num_frames == INT32_MAX, "maximum safe coefficient frame count");
    auto frame = result->GetFrame(INT32_MAX - 2, env);
    require(snapshot(frame, result->GetVideoInfo()) == snapshot(source->GetFrame(4, env), source->GetVideoInfo()),
            "maximum safe coefficient frame mapping");
    ++cases;
  }
  reject("AssumeFPS", {source, 2147483647, 1, true}, "audio sample rate");
  reject("AssumeScaledFPS", {source, 2147483647, 1, true}, "audio sample rate");
  PClip yuy2(new Sequence(env, VideoInfo::CS_YUY2, 32, 8, false));
  reject("ConvertFPS", {yuy2, 1, 1, 0}, "Scan-line advance");
  for (double fps : {0.0005, 1000.5, 4290676.5}) {
    const AVSValue args[] = {source, fps};
    PClip result = env->Invoke("AssumeFPS", AVSValue(args, 2)).AsClip();
    require(result->GetVideoInfo().fps_numerator > 0, "safe representable rate rejected");
    ++cases;
  }
}
void run_mode(Runtime& runtime, const char* plugin, const char* mode) {
  Environment reference_holder(runtime);
  auto* reference_env = reference_holder.env;
  Environment holder(runtime);
  auto* env = holder.env;
  try {
    reference_env->Invoke("SetMaxCPU", mode);
    env->Invoke("SetMaxCPU", mode);
    env->Invoke("LoadPlugin", plugin);
    check_rate_limits(env);
    for (int type : {VideoInfo::CS_BGR32, VideoInfo::CS_YUY2, VideoInfo::CS_Y8, VideoInfo::CS_YV12,
                     VideoInfo::CS_YUV444P16, VideoInfo::CS_YUV444PS, VideoInfo::CS_RGBAP16, VideoInfo::CS_RGBAPS}) {
      for (int width : {32, 66}) {
        PClip source(new Sequence(env, type, width, 32, false));
        for (bool option : {false, true}) {
          compare(reference_env, env, "AssumeScaledFPS", {source, 3, 2, option});
          for (const char* name : {"AssumeFPS", "ChangeFPS"}) {
            compare(reference_env, env, name, {source, 30000, 1001, option});
            compare(reference_env, env, name, {source, 29.97, option});
            compare(reference_env, env, name, {source, "film", option});
            compare(reference_env, env, name, {source, source, option});
          }
        }
        for (int numerator : {20, 30, 50, 60})
          compare(reference_env, env, "ConvertFPS", {source, numerator, 1});
        compare(reference_env, env, "ConvertFPS", {source, 29.97});
        compare(reference_env, env, "ConvertFPS", {source, "film"});
        compare(reference_env, env, "ConvertFPS", {source, source});
        if (type == VideoInfo::CS_YUY2)
          for (int zone : {0, 4, 16})
            for (int vbi : {0, 8})
              compare(reference_env, env, "ConvertFPS", {source, 50, 1, zone, vbi});
      }
    }
    for (const char* name : {"ContinuedNumerator", "ContinuedDenominator"}) {
      for (double value : {0.5, 23.976, 29.97, 59.94, 1000.125}) {
        for (int limit : {1, 1001, 10000}) {
          AVSValue args[] = {value, AVSValue(), limit};
          require(env->Invoke(name, AVSValue(args, 3)).AsInt() ==
                      reference_env->Invoke(name, AVSValue(args, 3)).AsInt(),
                  "continued fraction mismatch");
          ++cases;
        }
      }
    }
    std::printf("mode '%s' passed (%d cases so far)\n", mode, cases);
  } catch (const AvisynthError& e) {
    // Copy host-owned error text before destroying the environment.
    throw std::runtime_error(e.msg);
  }
}
} // namespace

int main(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc != 3)
    return 2;
  Runtime runtime(argv[1]);
  if (!runtime.module) {
    std::fprintf(stderr, "cannot load runtime: %s\n", argv[1]);
    return 2;
  }
  try {
    run_mode(runtime, argv[2], "none");
    {
      Environment holder(runtime);
      const int flags = holder.env->GetCPUFlags();
      if (flags & CPUF_SSE2)
        run_mode(runtime, argv[2], "sse2");
      if (flags & CPUF_SSSE3)
        run_mode(runtime, argv[2], "ssse3");
      if (flags & CPUF_SSE4_1)
        run_mode(runtime, argv[2], "sse4.1");
      if (flags & CPUF_AVX2)
        run_mode(runtime, argv[2], "avx2");
    }
    run_mode(runtime, argv[2], "");
    std::printf("%d runtime differential and rate-boundary cases passed\n", cases);
  } catch (const AvisynthError& e) {
    std::fprintf(stderr, "AviSynth: %s\n", e.msg);
    return 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "case %d: %s\n", cases, e.what());
    return 1;
  }
}
