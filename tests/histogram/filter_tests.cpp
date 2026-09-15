// SPDX-License-Identifier: GPL-2.0-or-later
// Runtime differential tests: no link dependency on AvsCore or its private headers.
#define NOMINMAX
#include <avisynth.h>
#include "../common/reference_clip.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
  Sequence(IScriptEnvironment* env, int type, int width, int height, bool stable, bool positive_chroma = false) {
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
              reinterpret_cast<float*>(row)[x] =
                  positive_chroma && (p == PLANAR_U || p == PLANAR_V)
                      ? 0.5f
                      : ((vi_.IsYUV() || vi_.IsYUVA()) && (p == PLANAR_U || p == PLANAR_V) ? float(value) / 256.0f - .5f
                                                                                           : float(value) / 191 - .25f);
          }
      }
      env->propSetInt(env->getFramePropsRW(f), "AIFTest", 700 + n, PROPAPPENDMODE_REPLACE);
      frames_.push_back(f);
    }
  }
  int __stdcall GetVersion() override { return AVISYNTH_INTERFACE_VERSION; }
  const VideoInfo& __stdcall GetVideoInfo() override { return vi_; }
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment*) override { return frames_.at(n); }
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
  const auto reference_args = reference_arguments(args, env, reference_env);
  PClip old_filter = reference_env->Invoke(name, AVSValue(reference_args.data(), int(reference_args.size()))).AsClip();
  PClip new_filter = env->Invoke(migrated.c_str(), AVSValue(args.data(), int(args.size()))).AsClip();
  const auto& out = new_filter->GetVideoInfo();
  const auto& ref = old_filter->GetVideoInfo();
  require(out.width == ref.width && out.height == ref.height && out.pixel_type == ref.pixel_type &&
              out.num_frames == ref.num_frames && out.num_audio_samples == ref.num_audio_samples,
          "metadata changed");
  for (int n : {4, 0, 2, 2}) {
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
    require(new_filter->GetParity(n) == source->GetParity(n), "parity changed");
  }
  int16_t old_audio[32], new_audio[32];
  old_filter->GetAudio(old_audio, 31, 16, reference_env);
  new_filter->GetAudio(new_audio, 31, 16, env);
  require(std::memcmp(old_audio, new_audio, sizeof(old_audio)) == 0, "audio passthrough changed");
  for (int n = 0; n < vi.num_frames; ++n)
    require(snapshot(source->GetFrame(n, env), vi) == before[n], "source frame modified");
}

// The reference implementation leaves new alpha regions uninitialized, so use
// explicit pixel expectations rather than its output for these cases.
void check_alpha(IScriptEnvironment* env) {
  for (int type : {VideoInfo::CS_YUVA444, VideoInfo::CS_YUVA444P16, VideoInfo::CS_YUVA444PS, VideoInfo::CS_RGBAP,
                   VideoInfo::CS_RGBAP16, VideoInfo::CS_RGBAPS}) {
    PClip source(new Sequence(env, type, 320, 128, false));
    const auto& vi = source->GetVideoInfo();
    std::vector<std::vector<uint8_t>> before;
    for (int n = 0; n < vi.num_frames; ++n)
      before.push_back(snapshot(source->GetFrame(n, env), vi));
    for (const char* name : {"Classic", "Levels", "Color", "Color2", "Luma", "StereoOverlay", "AudioLevels"}) {
      const std::string mode(name);
      if (vi.IsRGB() && mode != "Levels")
        continue;
      if (vi.ComponentSize() != 1 && (mode == "StereoOverlay" || mode == "AudioLevels"))
        continue;
      for (bool keep : {false, true}) {
        ++cases;
        const AVSValue args[] = {source, name, AVSValue(), 8, keep, true};
        PClip filter = env->Invoke("Histogram", AVSValue(args, 6)).AsClip();
        const bool preserve = mode != "Luma" && (keep || mode == "StereoOverlay" || mode == "AudioLevels");
        const int size = vi.ComponentSize();
        const uint8_t opaque8 = 255;
        const uint16_t opaque16 = uint16_t((1u << (size == 2 ? vi.BitsPerComponent() : 8)) - 1);
        const float opaque32 = 1.0f;
        const void* opaque = size == 1   ? static_cast<const void*>(&opaque8)
                             : size == 2 ? static_cast<const void*>(&opaque16)
                                         : static_cast<const void*>(&opaque32);
        for (int n : {4, 0, 4}) {
          const PVideoFrame src = source->GetFrame(n, env);
          const PVideoFrame dst = filter->GetFrame(n, env);
          for (int y = 0; y < dst->GetHeight(PLANAR_A); ++y) {
            const auto* row = dst->GetReadPtr(PLANAR_A) + y * dst->GetPitch(PLANAR_A);
            for (int x = 0; x < dst->GetRowSize(PLANAR_A) / size; ++x) {
              const void* expected = opaque;
              if (preserve && x < vi.width && y < vi.height)
                expected = src->GetReadPtr(PLANAR_A) + y * src->GetPitch(PLANAR_A) + x * size;
              if (std::memcmp(row + x * size, expected, size) != 0) {
                std::fprintf(stderr, "alpha mismatch: type=%d mode=%s keep=%d frame=%d x=%d y=%d\n", type, name, keep,
                             n, x, y);
                throw std::runtime_error("alpha preservation or opaque output changed");
              }
            }
          }
        }
      }
    }
    for (int n = 0; n < vi.num_frames; ++n)
      require(snapshot(source->GetFrame(n, env), vi) == before[n], "alpha tests modified source frame");
  }
}

void check_color2_positive_boundary(IScriptEnvironment* env) {
  PClip source(new Sequence(env, VideoInfo::CS_YUVA444PS, 4, 4, false, true));
  const auto& vi = source->GetVideoInfo();
  const PVideoFrame src = source->GetFrame(0, env);
  const auto before = snapshot(src, vi);
  for (int bits : {8, 9}) {
    ++cases;
    const AVSValue args[] = {source, "Color2", AVSValue(), bits, false, false};
    PClip filter = env->Invoke("Histogram", AVSValue(args, 6)).AsClip();
    const PVideoFrame dst = filter->GetFrame(0, env);
    const int last = (1 << bits) - 1;
    // All samples have U=V=+0.5: they belong in the final active pixel,
    // with the last source sample winning, never in row/column show_size.
    for (int plane : {PLANAR_Y, PLANAR_U, PLANAR_V}) {
      const float expected = reinterpret_cast<const float*>(src->GetReadPtr(plane) + 3 * src->GetPitch(plane))[3];
      const float actual = reinterpret_cast<const float*>(dst->GetReadPtr(plane) + last * dst->GetPitch(plane))[last];
      require(actual == expected, "Color2 +0.5 endpoint did not map to final active pixel");
    }
    for (int y = 0; y <= last; ++y) {
      const auto* alpha = reinterpret_cast<const float*>(dst->GetReadPtr(PLANAR_A) + y * dst->GetPitch(PLANAR_A));
      for (int x = 0; x <= last; ++x)
        require(alpha[x] == 1.0f, "Color2 endpoint corrupted alpha");
    }
  }
  require(snapshot(source->GetFrame(0, env), vi) == before, "Color2 endpoint modified source");
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
    for (int type : {VideoInfo::CS_Y8, VideoInfo::CS_YV12, VideoInfo::CS_YV24, VideoInfo::CS_YUV444P16,
                     VideoInfo::CS_YUV444PS, VideoInfo::CS_RGBP}) {
      PClip source(new Sequence(env, type, 320, 256, false));
      for (const char* mode :
           {"Classic", "Levels", "Color", "Color2", "Luma", "Stereo", "StereoOverlay", "AudioLevels"}) {
        const auto& vi = source->GetVideoInfo();
        if (vi.IsRGB() && std::string(mode) != "Levels")
          continue;
        if (vi.IsY() &&
            (std::string(mode) == "Color" || std::string(mode) == "Color2" || std::string(mode) == "AudioLevels"))
          continue;
        if (vi.ComponentSize() != 1 && (std::string(mode) == "Stereo" || std::string(mode) == "StereoOverlay" ||
                                        std::string(mode) == "AudioLevels"))
          continue;
        for (bool keepsource : {false, true}) {
          try {
            compare(reference_env, env, "Histogram", {source, mode, AVSValue(), 8, keepsource, true});
          } catch (...) {
            std::fprintf(stderr, "type=%d mode=%s keep=%d\n", type, mode, keepsource);
            throw;
          }
        }
      }
    }
    check_alpha(env);
    check_color2_positive_boundary(env);
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
    std::printf("%d runtime differential and alpha cases passed\n", cases);
  } catch (const AvisynthError& e) {
    std::fprintf(stderr, "AviSynth: %s\n", e.msg);
    return 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "case %d: %s\n", cases, e.what());
    return 1;
  }
}
