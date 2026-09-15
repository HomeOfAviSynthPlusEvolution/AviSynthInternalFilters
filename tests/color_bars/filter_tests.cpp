// SPDX-License-Identifier: GPL-2.0-or-later
// Runtime differential tests: no link dependency on AvsCore or its private headers.
#define NOMINMAX
#include <avisynth.h>
#include "../common/reference_clip.h"
#include "../../src/color_bars/kernel_adapter.h"
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
using aif::filters::color_bars::allowed_cpu_flags;
constexpr uint64_t base_flags = CPUF_SSE2 | CPUF_SSSE3 | CPUF_SSE4_1 | CPUF_SSE4_2 | CPUF_AES;
constexpr uint64_t avx2_flags = base_flags | CPUF_AVX | CPUF_AVX2 | CPUF_FMA3 | CPUF_F16C;
constexpr uint64_t avx3_flags =
    avx2_flags | CPUF_AVX512F | CPUF_AVX512CD | CPUF_AVX512BW | CPUF_AVX512DQ | CPUF_AVX512VL;
constexpr uint64_t dl_flags =
    avx3_flags | CPUF_AVX512VNNI | CPUF_AVX512VBMI | CPUF_AVX512VBMI2 | CPUF_AVX512BITALG | CPUF_AVX512VPOPCNTDQ;
static_assert(allowed_cpu_flags(0) == 0);
static_assert(allowed_cpu_flags(CPUF_SSE2) == AIF_COLOR_BARS_SSE2);
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
  Sequence(IScriptEnvironment* env, int type, int width, int height, bool stable) {
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

void compare(IScriptEnvironment* reference_env, IScriptEnvironment* env, const char* name, std::vector<AVSValue> args) {
  ++cases;
  const auto reference_args = reference_arguments(args, env, reference_env);
  PClip a = reference_env->Invoke(name, AVSValue(reference_args.data(), int(reference_args.size()))).AsClip();
  PClip b = env->Invoke((std::string(name)).c_str(), AVSValue(args.data(), int(args.size()))).AsClip();
  const auto& x = a->GetVideoInfo();
  const auto& y = b->GetVideoInfo();
  require(x.width == y.width && x.height == y.height && x.pixel_type == y.pixel_type && x.num_frames == y.num_frames &&
              x.num_audio_samples == y.num_audio_samples && x.sample_type == y.sample_type &&
              x.nchannels == y.nchannels && x.fps_numerator == y.fps_numerator &&
              x.fps_denominator == y.fps_denominator,
          "metadata mismatch");
  for (int n : {0, 2, 4}) {
    if (x.HasVideo()) {
      auto af = a->GetFrame(n, reference_env), bf = b->GetFrame(n, env);
      require(snapshot(af, x) == snapshot(bf, y), "blank pixels mismatch");
      for (const char* prop : {"_Matrix", "_ColorRange"})
        require(env->propGetInt(env->getFramePropsRO(af), prop, 0, nullptr) ==
                    env->propGetInt(env->getFramePropsRO(bf), prop, 0, nullptr),
                "frame property mismatch");
    }
    require(a->GetParity(n) == b->GetParity(n), "parity mismatch");
  }
  if (x.HasAudio()) {
    std::vector<uint8_t> aa(size_t(x.BytesFromAudioSamples(17)), 0xAD), bb(aa);
    a->GetAudio(aa.data(), 5, 17, reference_env);
    b->GetAudio(bb.data(), 5, 17, env);
    require(aa == bb, "silence mismatch");
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
    for (const char* type : {"YV24", "YV12", "YV16", "YV411", "YUV444P10", "YUV444PS", "YUVA420P16", "RGBP", "RGBAP16",
                             "RGBAPS", "RGB24", "RGB32", "RGB48", "RGB64", "YUY2"})
      for (int width : {16, 68, 280})
        for (bool stable : {false, true})
          compare(reference_env, env, "ColorBars", {width, 24, type, stable});
    for (const char* type : {"YV24", "YUV444P10", "YUV444P12", "YUV444P14", "YUV444P16", "YUV444PS", "YUVA444P16"})
      for (int width : {28, 68, 280})
        for (bool stable : {false, true})
          compare(reference_env, env, "ColorBarsHD", {width, 24, type, stable});
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
