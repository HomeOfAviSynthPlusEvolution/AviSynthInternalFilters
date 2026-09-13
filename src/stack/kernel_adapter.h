// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include "blank_clip/kernel.h"
namespace aif::filters::stack {
inline constexpr uint32_t allowed_cpu_flags(uint64_t flags) {
#if defined(ARM64) || defined(ARM32)
  return flags & CPUF_ARM_NEON ? AIF_BLANK_CLIP_NEON : 0;
#else
  uint32_t cpu = (flags & CPUF_SSE2 ? AIF_BLANK_CLIP_SSE2 : 0) | (flags & CPUF_SSSE3 ? AIF_BLANK_CLIP_SSSE3 : 0);
  const auto sse4 = CPUF_SSE4_1 | CPUF_SSE4_2 | CPUF_AES;
  if ((flags & sse4) != sse4)
    return cpu;
  cpu |= AIF_BLANK_CLIP_SSE4;
  const auto avx2 = CPUF_AVX | CPUF_AVX2 | CPUF_FMA3 | CPUF_F16C;
  if ((flags & avx2) != avx2)
    return cpu;
  cpu |= AIF_BLANK_CLIP_AVX2;
  const auto avx3 = CPUF_AVX512F | CPUF_AVX512CD | CPUF_AVX512BW | CPUF_AVX512DQ | CPUF_AVX512VL;
  if ((flags & avx3) != avx3)
    return cpu;
  cpu |= AIF_BLANK_CLIP_AVX3;
  const auto dl = CPUF_AVX512VNNI | CPUF_AVX512VBMI | CPUF_AVX512VBMI2 | CPUF_AVX512BITALG | CPUF_AVX512VPOPCNTDQ;
  if ((flags & dl) != dl)
    return cpu;
  cpu |= AIF_BLANK_CLIP_AVX3_DL;
  if (flags & CPUF_AVX512BF16) {
    cpu |= AIF_BLANK_CLIP_AVX3_ZEN4;
    if (flags & CPUF_AVX512FP16)
      cpu |= AIF_BLANK_CLIP_AVX3_SPR;
  }
  return cpu;
#endif
}
inline uint32_t allowed_cpu(IScriptEnvironment* env) {
  return allowed_cpu_flags(env->GetCPUFlagsEx());
}
} // namespace aif::filters::stack
