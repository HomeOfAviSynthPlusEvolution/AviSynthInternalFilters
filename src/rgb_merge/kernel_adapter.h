// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include "rgb_merge/kernel.h"
#include <cctype>
namespace aif::filters::rgb_merge {
inline int lstrcmpi(const char* a, const char* b) {
  for (; *a && *b; ++a, ++b) {
    int d = std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
    if (d)
      return d;
  }
  return *a - *b;
}
inline int pixel_type_id(const char* name, IScriptEnvironment* env) {
  try {
    AVSValue args[] = {16, 16, 1, name};
    const char* names[] = {"width", "height", "length", "pixel_type"};
    return env->Invoke("BlankClip", AVSValue(args, 4), names).AsClip()->GetVideoInfo().pixel_type;
  } catch (const AvisynthError&) {
    return VideoInfo::CS_UNKNOWN;
  }
}
inline constexpr uint32_t allowed_cpu_flags(uint64_t flags) {
#if defined(ARM64) || defined(ARM32)
  return flags & CPUF_ARM_NEON ? AIF_RGB_MERGE_NEON : 0;
#else
  uint32_t cpu = (flags & CPUF_SSE2 ? AIF_RGB_MERGE_SSE2 : 0) | (flags & CPUF_SSSE3 ? AIF_RGB_MERGE_SSSE3 : 0);
  const auto sse4 = CPUF_SSE4_1 | CPUF_SSE4_2 | CPUF_AES;
  if ((flags & sse4) != sse4)
    return cpu;
  cpu |= AIF_RGB_MERGE_SSE4;
  const auto avx2 = CPUF_AVX | CPUF_AVX2 | CPUF_FMA3 | CPUF_F16C;
  if ((flags & avx2) != avx2)
    return cpu;
  cpu |= AIF_RGB_MERGE_AVX2;
  const auto avx3 = CPUF_AVX512F | CPUF_AVX512CD | CPUF_AVX512BW | CPUF_AVX512DQ | CPUF_AVX512VL;
  if ((flags & avx3) != avx3)
    return cpu;
  cpu |= AIF_RGB_MERGE_AVX3;
  const auto dl = CPUF_AVX512VNNI | CPUF_AVX512VBMI | CPUF_AVX512VBMI2 | CPUF_AVX512BITALG | CPUF_AVX512VPOPCNTDQ;
  if ((flags & dl) != dl)
    return cpu;
  cpu |= AIF_RGB_MERGE_AVX3_DL;
  if (flags & CPUF_AVX512BF16) {
    cpu |= AIF_RGB_MERGE_AVX3_ZEN4;
    if (flags & CPUF_AVX512FP16)
      cpu |= AIF_RGB_MERGE_AVX3_SPR;
  }
  return cpu;
#endif
}
inline uint32_t cpu(IScriptEnvironment* env) {
  return allowed_cpu_flags(env->GetCPUFlagsEx());
}
} // namespace aif::filters::rgb_merge
