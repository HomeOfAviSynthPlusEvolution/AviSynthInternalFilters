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
uint32_t aif::channel_display::packed_supported_cpu() {
#ifndef AIF_SCALAR_ONLY
  const auto t = hwy::SupportedTargets();
  uint32_t cpu = 0;
#if HWY_TARGETS & HWY_SSE2
  if (t & HWY_SSE2)
    cpu |= AIF_CHANNEL_DISPLAY_SSE2;
#endif
#if HWY_TARGETS & HWY_SSSE3
  if (t & HWY_SSSE3)
    cpu |= AIF_CHANNEL_DISPLAY_SSSE3;
#endif
#if HWY_TARGETS & HWY_SSE4
  if (t & HWY_SSE4)
    cpu |= AIF_CHANNEL_DISPLAY_SSE4;
#endif
#if HWY_TARGETS & HWY_AVX2
  if (t & HWY_AVX2)
    cpu |= AIF_CHANNEL_DISPLAY_AVX2;
#endif
#if HWY_TARGETS & HWY_AVX3
  if (t & HWY_AVX3)
    cpu |= AIF_CHANNEL_DISPLAY_AVX3;
#endif
#if HWY_TARGETS & HWY_AVX3_DL
  if (t & HWY_AVX3_DL)
    cpu |= AIF_CHANNEL_DISPLAY_AVX3_DL;
#endif
#if HWY_TARGETS & HWY_AVX3_ZEN4
  if (t & HWY_AVX3_ZEN4)
    cpu |= AIF_CHANNEL_DISPLAY_AVX3_ZEN4;
#endif
#if HWY_TARGETS & HWY_AVX3_SPR
  if (t & HWY_AVX3_SPR)
    cpu |= AIF_CHANNEL_DISPLAY_AVX3_SPR;
#endif
#if HWY_TARGETS & HWY_AVX10_2
  if (t & HWY_AVX10_2)
    cpu |= AIF_CHANNEL_DISPLAY_AVX10_2;
#endif
#if HWY_TARGETS & HWY_NEON_WITHOUT_AES
  if (t & HWY_NEON_WITHOUT_AES)
    cpu |= AIF_CHANNEL_DISPLAY_NEON;
#endif
  return cpu;
#else
  return 0;
#endif
}
aif::channel_display::PackedRow aif::channel_display::backend(uint32_t cpu) {
  cpu &= aif_channel_display_supported_cpu();
#ifndef AIF_SCALAR_ONLY
#if HWY_TARGETS & HWY_AVX10_2
  if (cpu & AIF_CHANNEL_DISPLAY_AVX10_2)
    return HWY_CHOOSE_AVX10_2(RenderPacked);
#endif
#if HWY_TARGETS & HWY_AVX3_SPR
  if (cpu & AIF_CHANNEL_DISPLAY_AVX3_SPR)
    return HWY_CHOOSE_AVX3_SPR(RenderPacked);
#endif
#if HWY_TARGETS & HWY_AVX3_ZEN4
  if (cpu & AIF_CHANNEL_DISPLAY_AVX3_ZEN4)
    return HWY_CHOOSE_AVX3_ZEN4(RenderPacked);
#endif
#if HWY_TARGETS & HWY_AVX3_DL
  if (cpu & AIF_CHANNEL_DISPLAY_AVX3_DL)
    return HWY_CHOOSE_AVX3_DL(RenderPacked);
#endif
#if HWY_TARGETS & HWY_AVX3
  if (cpu & AIF_CHANNEL_DISPLAY_AVX3)
    return HWY_CHOOSE_AVX3(RenderPacked);
#endif
#if HWY_TARGETS & HWY_AVX2
  if (cpu & AIF_CHANNEL_DISPLAY_AVX2)
    return HWY_CHOOSE_AVX2(RenderPacked);
#endif
#if HWY_TARGETS & HWY_SSE4
  if (cpu & AIF_CHANNEL_DISPLAY_SSE4)
    return HWY_CHOOSE_SSE4(RenderPacked);
#endif
#if HWY_TARGETS & HWY_SSSE3
  if (cpu & AIF_CHANNEL_DISPLAY_SSSE3)
    return HWY_CHOOSE_SSSE3(RenderPacked);
#endif
#if HWY_TARGETS & HWY_SSE2
  if (cpu & AIF_CHANNEL_DISPLAY_SSE2)
    return HWY_CHOOSE_SSE2(RenderPacked);
#endif
#if HWY_TARGETS & HWY_NEON_WITHOUT_AES
  if (cpu & AIF_CHANNEL_DISPLAY_NEON)
    return HWY_CHOOSE_NEON_WITHOUT_AES(RenderPacked);
#endif
#else
  (void)cpu;
#endif
  return nullptr;
}
#endif
