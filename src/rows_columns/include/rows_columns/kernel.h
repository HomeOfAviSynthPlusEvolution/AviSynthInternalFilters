// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_ROWS_COLUMNS_SSE2 = 1,
  AIF_ROWS_COLUMNS_SSSE3 = 2,
  AIF_ROWS_COLUMNS_NEON = 4,
  AIF_ROWS_COLUMNS_SSE4 = 8,
  AIF_ROWS_COLUMNS_AVX2 = 16,
  AIF_ROWS_COLUMNS_AVX3 = 32,
  AIF_ROWS_COLUMNS_AVX3_DL = 64,
  AIF_ROWS_COLUMNS_AVX3_ZEN4 = 128,
  AIF_ROWS_COLUMNS_AVX3_SPR = 256,
  AIF_ROWS_COLUMNS_AVX10_2 = 512
};
uint32_t aif_rows_columns_supported_cpu(void);
/* count is the narrow image pixel width; size is pixel bytes 1,2,3,4,6,8,
 * or 0 for YUY2 (even count). Positive pitches, naturally aligned samples, disjoint output; no padding
 * access. Separate takes one wide source; weave takes period narrow sources.
 * phase is [0,period) for separate, ignored for weave. */
int aif_rows_columns_process(const uint8_t* const src[], const int sp[], uint8_t* dst, int dp, int count, int height,
                             int size, int period, int phase, int weave, uint32_t cpu);
#ifdef __cplusplus
}
#endif
