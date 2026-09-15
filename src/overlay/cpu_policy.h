#pragma once
#include "../common/host_cpu.h"
#include <composite/dispatch.h>
#ifndef AIF_SCALAR_ONLY
#include <hwy/detect_targets.h>
#endif
namespace aif::filters::overlay {
inline int64_t allowed_targets(uint64_t flags) {
#ifndef AIF_SCALAR_ONLY
#if defined(ARM64) || defined(ARM32)
  return flags & CPUF_ARM_NEON ? HWY_NEON_WITHOUT_AES : 0;
#else
  int64_t cpu = (flags & CPUF_SSE2 ? HWY_SSE2 : 0) | (flags & CPUF_SSSE3 ? HWY_SSSE3 : 0);
  const auto sse4 = CPUF_SSE4_1 | CPUF_SSE4_2 | CPUF_AES;
  if ((flags & sse4) != sse4)
    return cpu;
  cpu |= HWY_SSE4;
  const auto avx2 = CPUF_AVX | CPUF_AVX2 | CPUF_FMA3 | CPUF_F16C;
  if ((flags & avx2) != avx2)
    return cpu;
  cpu |= HWY_AVX2;
  const auto avx3 = CPUF_AVX512F | CPUF_AVX512CD | CPUF_AVX512BW | CPUF_AVX512DQ | CPUF_AVX512VL;
  if ((flags & avx3) != avx3)
    return cpu;
  cpu |= HWY_AVX3;
  const auto dl = CPUF_AVX512VNNI | CPUF_AVX512VBMI | CPUF_AVX512VBMI2 | CPUF_AVX512BITALG | CPUF_AVX512VPOPCNTDQ;
  if ((flags & dl) != dl)
    return cpu;
  cpu |= HWY_AVX3_DL;
  if (flags & CPUF_AVX512BF16) {
    cpu |= HWY_AVX3_ZEN4;
    if (flags & CPUF_AVX512FP16)
      cpu |= HWY_AVX3_SPR;
  }
  return cpu;
#endif
#else
  (void)flags;
  return 0;
#endif
}
inline const cp_kernels* select_kernels(IScriptEnvironment* env) {
  return cp_get_kernels(cp_choose_target(allowed_targets(aif::filters::host_cpu_flags(env))));
}
} // namespace aif::filters::overlay
