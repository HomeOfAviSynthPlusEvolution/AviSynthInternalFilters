// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_BLANK_CLIP_SSE2 = 1,
  AIF_BLANK_CLIP_SSSE3 = 2,
  AIF_BLANK_CLIP_NEON = 4,
  AIF_BLANK_CLIP_SSE4 = 8,
  AIF_BLANK_CLIP_AVX2 = 16,
  AIF_BLANK_CLIP_AVX3 = 32,
  AIF_BLANK_CLIP_AVX3_DL = 64,
  AIF_BLANK_CLIP_AVX3_ZEN4 = 128,
  AIF_BLANK_CLIP_AVX3_SPR = 256,
  AIF_BLANK_CLIP_AVX10_2 = 512
};
uint32_t aif_blank_clip_supported_cpu(void);
/* Fill visible rows, preserving padding. Positive pitch; complete samples or
 * packed pixels of 1,2,3,4,6,8 bytes. Pattern points to one encoded pixel. */
int aif_blank_clip_fill(uint8_t* dst, int pitch, int row_bytes, int height, const void* pattern, int pattern_bytes,
                        uint32_t cpu);
#ifdef __cplusplus
}
#endif
