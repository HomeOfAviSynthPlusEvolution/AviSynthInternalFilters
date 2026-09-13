// SPDX-License-Identifier: GPL-2.0-or-later
// Runtime differential tests: no link dependency on AvsCore or its private headers.
#define NOMINMAX
#include <avisynth.h>
#include "../../src/greyscale/kernel_adapter.h"
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

#if !defined(ARM64) && !defined(ARM32)
using aif::filters::greyscale::allowed_cpu_flags;
constexpr uint64_t base_flags = CPUF_SSE2 | CPUF_SSSE3 | CPUF_SSE4_1 | CPUF_SSE4_2 | CPUF_AES;
constexpr uint64_t avx2_flags = base_flags | CPUF_AVX | CPUF_AVX2 | CPUF_FMA3 | CPUF_F16C;
constexpr uint64_t avx3_flags =
    avx2_flags | CPUF_AVX512F | CPUF_AVX512CD | CPUF_AVX512BW | CPUF_AVX512DQ | CPUF_AVX512VL;
constexpr uint64_t dl_flags =
    avx3_flags | CPUF_AVX512VNNI | CPUF_AVX512VBMI | CPUF_AVX512VBMI2 | CPUF_AVX512BITALG | CPUF_AVX512VPOPCNTDQ;
static_assert(allowed_cpu_flags(0) == 0);
static_assert(allowed_cpu_flags(CPUF_SSE2) == AIF_GREYSCALE_SSE2);
static_assert(allowed_cpu_flags(base_flags) == (1 | 2 | 8));
static_assert(allowed_cpu_flags(avx2_flags) == (1 | 2 | 8 | 16));
static_assert(allowed_cpu_flags(avx3_flags) == (1 | 2 | 8 | 16 | 32));
static_assert(allowed_cpu_flags(dl_flags) == (1 | 2 | 8 | 16 | 32 | 64));
static_assert(allowed_cpu_flags(dl_flags | CPUF_AVX512BF16) == (1 | 2 | 8 | 16 | 32 | 64 | 128));
static_assert(allowed_cpu_flags(dl_flags | CPUF_AVX512BF16 | CPUF_AVX512FP16) ==
              (1 | 2 | 8 | 16 | 32 | 64 | 128 | 256));
static_assert(allowed_cpu_flags(avx2_flags & ~uint64_t(CPUF_FMA3)) == (1 | 2 | 8));
static_assert(allowed_cpu_flags(avx3_flags & ~uint64_t(CPUF_AVX512BW)) == (1 | 2 | 8 | 16));
#endif

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
  Sequence(IScriptEnvironment* env, int type, int width, int height, bool stable, int range = -1) {
    vi_.width = width;
    vi_.height = height;
    vi_.pixel_type = type;
    vi_.num_frames = 5;
    vi_.fps_numerator = 25;
    vi_.fps_denominator = 1;
    vi_.audio_samples_per_second = 48000;
    vi_.nchannels = 1;
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
      if (range >= 0)
        env->propSetInt(env->getFramePropsRW(f), "_ColorRange", range, PROPAPPENDMODE_REPLACE);
      env->propSetInt(env->getFramePropsRW(f), "_Matrix", 1, PROPAPPENDMODE_REPLACE);
      frames_.push_back(f);
    }
  }
  int __stdcall GetVersion() override { return AVISYNTH_INTERFACE_VERSION; }
  const VideoInfo& __stdcall GetVideoInfo() override { return vi_; }
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment*) override { return frames_.at(n); }
  bool __stdcall GetParity(int n) override { return (n & 1) != 0; }
  void __stdcall GetAudio(void* buf, int64_t start, int64_t count, IScriptEnvironment*) override {
    auto* samples = static_cast<int16_t*>(buf);
    for (int64_t i = 0; i < count; ++i)
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

void compare(IScriptEnvironment* env, const char* name, const std::vector<AVSValue>& args) {
  ++cases;
  const std::string migrated = std::string("IF") + name;
  PClip source = args[0].AsClip();
  const auto& vi = source->GetVideoInfo();
  std::vector<std::vector<uint8_t>> before;
  for (int n = 0; n < vi.num_frames; ++n)
    before.push_back(snapshot(source->GetFrame(n, env), vi));
  PClip old_filter = env->Invoke(name, AVSValue(args.data(), int(args.size()))).AsClip();
  PClip new_filter = env->Invoke(migrated.c_str(), AVSValue(args.data(), int(args.size()))).AsClip();
  const auto& out = new_filter->GetVideoInfo();
  const auto& ref = old_filter->GetVideoInfo();
  require(out.width == ref.width && out.height == ref.height && out.pixel_type == vi.pixel_type &&
              out.num_frames == vi.num_frames && out.num_audio_samples == vi.num_audio_samples,
          "metadata changed");
  for (int n : {4, 0, 2, 2}) {
    PVideoFrame a = old_filter->GetFrame(n, env), b = new_filter->GetFrame(n, env);
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
    for (const char* key : {"_ColorRange", "_Matrix"}) {
      int ea = 0, eb = 0;
      auto va = env->propGetInt(env->getFramePropsRO(a), key, 0, &ea);
      auto vb = env->propGetInt(env->getFramePropsRO(b), key, 0, &eb);
      if (ea != eb || (!ea && va != vb))
        std::fprintf(stderr, "%s: old=%lld error=%d new=%lld error=%d\n", key, static_cast<long long>(va), ea,
                     static_cast<long long>(vb), eb);
      require(ea == eb && (ea || va == vb), "color metadata differs");
    }
    require(env->propGetInt(env->getFramePropsRO(b), "AIFTest", 0, &error) == 700 + n && !error,
            "frame properties lost");
    require(new_filter->GetParity(n) == source->GetParity(n), "parity changed");
  }
  int16_t old_audio[16], new_audio[16];
  old_filter->GetAudio(old_audio, 31, 16, env);
  new_filter->GetAudio(new_audio, 31, 16, env);
  require(std::memcmp(old_audio, new_audio, sizeof(old_audio)) == 0, "audio passthrough changed");
  for (int n = 0; n < vi.num_frames; ++n)
    require(snapshot(source->GetFrame(n, env), vi) == before[n], "source frame modified");
}

void run_mode(Runtime& runtime, const char* plugin, const char* mode) {
  Environment holder(runtime);
  auto* env = holder.env;
  try {
    env->Invoke("SetMaxCPU", mode);
    env->Invoke("LoadPlugin", plugin);
    const int types[] = {VideoInfo::CS_Y8,       VideoInfo::CS_Y10,        VideoInfo::CS_Y16,   VideoInfo::CS_Y32,
                         VideoInfo::CS_YV12,     VideoInfo::CS_YV16,       VideoInfo::CS_YV411, VideoInfo::CS_YUV444P10,
                         VideoInfo::CS_YUV444PS, VideoInfo::CS_YUVA420P16, VideoInfo::CS_RGBP,  VideoInfo::CS_RGBAP16,
                         VideoInfo::CS_RGBAPS,   VideoInfo::CS_BGR24,      VideoInfo::CS_BGR32, VideoInfo::CS_BGR48,
                         VideoInfo::CS_BGR64,    VideoInfo::CS_YUY2};
    for (int type : types)
      for (int width : {4, 16, 20, 64, 68})
        for (int height : {4, 8, 20}) {
          PClip source(new Sequence(env, type, width, height, false));
          compare(env, "Greyscale", {source});
          if (source->GetVideoInfo().IsRGB())
            for (const char* matrix :
                 {"Rec601", "Rec709", "Rec2020", "Average", "709:full", "601:limited", "rgb", "2020:same"})
              compare(env, "Grayscale", {source, matrix});
        }
    for (int type : {VideoInfo::CS_RGBP, VideoInfo::CS_RGBAP16, VideoInfo::CS_RGBAPS, VideoInfo::CS_BGR32})
      for (int range : {0, 1}) {
        PClip source(new Sequence(env, type, 17, 3, false, range));
        compare(env, "Greyscale", {source});
        for (const char* matrix :
             {"PC.601", "PC709", "Rec709", "709:same", "709:full", "709:limited", "auto", "Average"})
          compare(env, "Greyscale", {source, matrix});
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
    std::printf("%d runtime differential cases passed\n", cases);
  } catch (const AvisynthError& e) {
    std::fprintf(stderr, "AviSynth: %s\n", e.msg);
    return 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "case %d: %s\n", cases, e.what());
    return 1;
  }
}
