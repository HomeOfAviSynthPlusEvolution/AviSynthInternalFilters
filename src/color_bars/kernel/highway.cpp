// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#ifndef AIF_SCALAR_ONLY
#include "highway_config.h"
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
#include "highway_inl.h"
#endif
#if defined(AIF_SCALAR_ONLY) || HWY_ONCE
extern "C" uint32_t aif_color_bars_supported_cpu(void) {
#ifndef AIF_SCALAR_ONLY
  const auto t = hwy::SupportedTargets();
  uint32_t cpu = 0;
#if HWY_TARGETS & HWY_SSE2
  if (t & HWY_SSE2)
    cpu |= AIF_COLOR_BARS_SSE2;
#endif
#if HWY_TARGETS & HWY_SSSE3
  if (t & HWY_SSSE3)
    cpu |= AIF_COLOR_BARS_SSSE3;
#endif
#if HWY_TARGETS & HWY_SSE4
  if (t & HWY_SSE4)
    cpu |= AIF_COLOR_BARS_SSE4;
#endif
#if HWY_TARGETS & HWY_AVX2
  if (t & HWY_AVX2)
    cpu |= AIF_COLOR_BARS_AVX2;
#endif
#if HWY_TARGETS & HWY_AVX3
  if (t & HWY_AVX3)
    cpu |= AIF_COLOR_BARS_AVX3;
#endif
#if HWY_TARGETS & HWY_AVX3_DL
  if (t & HWY_AVX3_DL)
    cpu |= AIF_COLOR_BARS_AVX3_DL;
#endif
#if HWY_TARGETS & HWY_AVX3_ZEN4
  if (t & HWY_AVX3_ZEN4)
    cpu |= AIF_COLOR_BARS_AVX3_ZEN4;
#endif
#if HWY_TARGETS & HWY_AVX3_SPR
  if (t & HWY_AVX3_SPR)
    cpu |= AIF_COLOR_BARS_AVX3_SPR;
#endif
#if HWY_TARGETS & HWY_AVX10_2
  if (t & HWY_AVX10_2)
    cpu |= AIF_COLOR_BARS_AVX10_2;
#endif
#if HWY_TARGETS & HWY_NEON_WITHOUT_AES
  if (t & HWY_NEON_WITHOUT_AES)
    cpu |= AIF_COLOR_BARS_NEON;
#endif
  return cpu;
#else
  return 0;
#endif
}
const aif::color_bars::Kernels* aif::color_bars::backend(uint32_t cpu) {
  cpu &= aif_color_bars_supported_cpu();
#ifndef AIF_SCALAR_ONLY
#if HWY_TARGETS & HWY_AVX10_2
  if (cpu & AIF_COLOR_BARS_AVX10_2)
    return (HWY_CHOOSE_AVX10_2(GetKernels))();
#endif
#if HWY_TARGETS & HWY_AVX3_SPR
  if (cpu & AIF_COLOR_BARS_AVX3_SPR)
    return (HWY_CHOOSE_AVX3_SPR(GetKernels))();
#endif
#if HWY_TARGETS & HWY_AVX3_ZEN4
  if (cpu & AIF_COLOR_BARS_AVX3_ZEN4)
    return (HWY_CHOOSE_AVX3_ZEN4(GetKernels))();
#endif
#if HWY_TARGETS & HWY_AVX3_DL
  if (cpu & AIF_COLOR_BARS_AVX3_DL)
    return (HWY_CHOOSE_AVX3_DL(GetKernels))();
#endif
#if HWY_TARGETS & HWY_AVX3
  if (cpu & AIF_COLOR_BARS_AVX3)
    return (HWY_CHOOSE_AVX3(GetKernels))();
#endif
#if HWY_TARGETS & HWY_AVX2
  if (cpu & AIF_COLOR_BARS_AVX2)
    return (HWY_CHOOSE_AVX2(GetKernels))();
#endif
#if HWY_TARGETS & HWY_SSE4
  if (cpu & AIF_COLOR_BARS_SSE4)
    return (HWY_CHOOSE_SSE4(GetKernels))();
#endif
#if HWY_TARGETS & HWY_SSSE3
  if (cpu & AIF_COLOR_BARS_SSSE3)
    return (HWY_CHOOSE_SSSE3(GetKernels))();
#endif
#if HWY_TARGETS & HWY_SSE2
  if (cpu & AIF_COLOR_BARS_SSE2)
    return (HWY_CHOOSE_SSE2(GetKernels))();
#endif
#if HWY_TARGETS & HWY_NEON_WITHOUT_AES
  if (cpu & AIF_COLOR_BARS_NEON)
    return (HWY_CHOOSE_NEON_WITHOUT_AES(GetKernels))();
#endif
#else
  (void)cpu;
#endif
  return nullptr;
}
#endif
