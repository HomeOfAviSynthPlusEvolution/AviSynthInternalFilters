// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_MASK_SSE2 = 1,
  AIF_MASK_SSSE3 = 2,
  AIF_MASK_NEON = 4,
  AIF_MASK_SSE4 = 8,
  AIF_MASK_AVX2 = 16,
  AIF_MASK_AVX3 = 32,
  AIF_MASK_AVX3_DL = 64,
  AIF_MASK_AVX3_ZEN4 = 128,
  AIF_MASK_AVX3_SPR = 256,
  AIF_MASK_AVX10_2 = 512
};
/* Packed BGRA8 rows, width >= 0 pixels. Complete pixels only; padding untouched.
 * op 0: RGB luma from source to destination alpha; op 1: color key destination;
 * op 2: reset destination alpha to color low byte. key/tolerance packed BGR8.
 * Source required only for op 0. Resolve once using the host-allowed CPU mask. */
typedef void (*aif_mask_row)(uint8_t*, const uint8_t*, int, int, uint32_t, uint32_t);
uint32_t aif_mask_supported_cpu(void);
aif_mask_row aif_mask_resolve(uint32_t cpu);
#ifdef __cplusplus
}
#endif
