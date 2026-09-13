// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_PLANES_SSE2 = 1,
  AIF_PLANES_SSSE3 = 2,
  AIF_PLANES_NEON = 4,
  AIF_PLANES_SSE4 = 8,
  AIF_PLANES_AVX2 = 16,
  AIF_PLANES_AVX3 = 32,
  AIF_PLANES_AVX3_DL = 64,
  AIF_PLANES_AVX3_ZEN4 = 128,
  AIF_PLANES_AVX3_SPR = 256,
  AIF_PLANES_AVX10_2 = 512
};
uint32_t aif_planes_supported_cpu(void);
/* Disjoint visible buffers with positive pitches. Width is a luma pixel count.
 * Swap accepts even YUY2 width; extract's width is the output width and source
 * has twice that width. Extract packed output requires even output width.
 * Assemble output width is a multiple of four; u/v YUY2 sources have half that width.
 * A null y supplies 126 luma. All operations preserve destination padding. */
int aif_planes_swap(const uint8_t* src, int sp, uint8_t* dst, int dp, int width, int height, uint32_t cpu);
int aif_planes_extract_uv(const uint8_t* src, int sp, uint8_t* dst, int dp, int width, int height, int v, int packed,
                          uint32_t cpu);
int aif_planes_assemble(const uint8_t* y, int yp, const uint8_t* u, int up, const uint8_t* v, int vp, uint8_t* dst,
                        int dp, int width, int height, uint32_t cpu);
int aif_planes_fill(uint8_t* dst, int pitch, int row, int height, const void* value, int bytes, uint32_t cpu);
#ifdef __cplusplus
}
#endif
