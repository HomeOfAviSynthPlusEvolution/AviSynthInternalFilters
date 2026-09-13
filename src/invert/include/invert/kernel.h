// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_INVERT_SSE2 = 1,
  AIF_INVERT_SSSE3 = 2,
  AIF_INVERT_NEON = 4,
  AIF_INVERT_SSE4 = 8,
  AIF_INVERT_AVX2 = 16,
  AIF_INVERT_AVX3 = 32,
  AIF_INVERT_AVX3_DL = 64,
  AIF_INVERT_AVX3_ZEN4 = 128,
  AIF_INVERT_AVX3_SPR = 256,
  AIF_INVERT_AVX10_2 = 512
};
uint32_t aif_invert_supported_cpu(void);
/* In-place, positive naturally aligned pitch; padding untouched. Integer mode
 * (-1) XORs one sample/pixel pattern (1,2,3,4,6,8 bytes); row is a multiple.
 * Float mode 0 (chroma) or 1 (RGB/Y/A) computes mode-value, pattern ignored,
 * pattern_bytes must be 4. Return 0 on success, 1 on invalid geometry/mode. */
int aif_invert_apply(uint8_t* data, int pitch, int row_bytes, int height, const void* pattern, int pattern_bytes,
                     int float_mode, uint32_t cpu);
#ifdef __cplusplus
}
#endif
