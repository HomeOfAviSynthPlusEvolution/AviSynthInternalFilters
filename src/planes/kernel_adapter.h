// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include "planes/kernel.h"
#include <algorithm>
#include <cstring>
#include <cctype>
namespace aif::filters::planes {
inline int lstrcmpi(const char* a, const char* b) {
  for (; *a && *b; ++a, ++b) {
    int d = std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
    if (d)
      return d;
  }
  return (unsigned char)*a - (unsigned char)*b;
}
inline constexpr uint32_t allowed_cpu_flags(uint64_t flags) {
#if defined(ARM64) || defined(ARM32)
  return flags & CPUF_ARM_NEON ? AIF_PLANES_NEON : 0;
#else
  uint32_t cpu = (flags & CPUF_SSE2 ? AIF_PLANES_SSE2 : 0) | (flags & CPUF_SSSE3 ? AIF_PLANES_SSSE3 : 0);
  const auto sse4 = CPUF_SSE4_1 | CPUF_SSE4_2 | CPUF_AES;
  if ((flags & sse4) != sse4)
    return cpu;
  cpu |= AIF_PLANES_SSE4;
  const auto avx2 = CPUF_AVX | CPUF_AVX2 | CPUF_FMA3 | CPUF_F16C;
  if ((flags & avx2) != avx2)
    return cpu;
  cpu |= AIF_PLANES_AVX2;
  const auto avx3 = CPUF_AVX512F | CPUF_AVX512CD | CPUF_AVX512BW | CPUF_AVX512DQ | CPUF_AVX512VL;
  if ((flags & avx3) != avx3)
    return cpu;
  cpu |= AIF_PLANES_AVX3;
  const auto dl = CPUF_AVX512VNNI | CPUF_AVX512VBMI | CPUF_AVX512VBMI2 | CPUF_AVX512BITALG | CPUF_AVX512VPOPCNTDQ;
  if ((flags & dl) != dl)
    return cpu;
  cpu |= AIF_PLANES_AVX3_DL;
  if (flags & CPUF_AVX512BF16) {
    cpu |= AIF_PLANES_AVX3_ZEN4;
    if (flags & CPUF_AVX512FP16)
      cpu |= AIF_PLANES_AVX3_SPR;
  }
  return cpu;
#endif
}
inline uint32_t cpu(IScriptEnvironment* env) {
  return allowed_cpu_flags(env->GetCPUFlagsEx());
}
inline PClip convert(PClip clip, const char* name, IScriptEnvironment* env) {
  return env->Invoke(name, clip).AsClip();
}
inline int pixel_type(const char* name, IScriptEnvironment* env) {
  AVSValue args[] = {16, 16, 1, name};
  const char* names[] = {"width", "height", "length", "pixel_type"};
  return env->Invoke("BlankClip", AVSValue(args, 4), names).AsClip()->GetVideoInfo().pixel_type;
}
template <class T>
void fill_plane(uint8_t* p, int h, int row, int pitch, T value, IScriptEnvironment* env) {
  if (aif_planes_fill(p, pitch, row, h, &value, sizeof(T), cpu(env)))
    env->ThrowError("Planes: invalid fill");
}
template <class T>
void fill_chroma(uint8_t* u, uint8_t* v, int h, int row, int pitch, T value, IScriptEnvironment* env) {
  fill_plane(u, h, row, pitch, value, env);
  fill_plane(v, h, row, pitch, value, env);
}
inline float uv8tof(int value) {
  return (value - 128) / 255.0f;
}
} // namespace aif::filters::planes
