// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_GREYSCALE_SSE2 = 1,
  AIF_GREYSCALE_SSSE3 = 2,
  AIF_GREYSCALE_NEON = 4,
  AIF_GREYSCALE_SSE4 = 8,
  AIF_GREYSCALE_AVX2 = 16,
  AIF_GREYSCALE_AVX3 = 32,
  AIF_GREYSCALE_AVX3_DL = 64,
  AIF_GREYSCALE_AVX3_ZEN4 = 128,
  AIF_GREYSCALE_AVX3_SPR = 256,
  AIF_GREYSCALE_AVX10_2 = 512
};
uint32_t aif_greyscale_supported_cpu(void);
typedef struct aif_greyscale_plan aif_greyscale_plan;
int aif_greyscale_create(double kr, double kb, int bits, int source_full, int destination_full, uint32_t cpu,
                         aif_greyscale_plan** output);
void aif_greyscale_destroy(aif_greyscale_plan* plan);
/* Positive naturally aligned pitches, visible rows only. Planar RGB pointers are
 * R,G,B; packed BGR(A) uses slot zero. Integer samples fit the declared depth.
 * Alpha and padding are preserved. The immutable plan can be used concurrently. */
int aif_greyscale_rgb(const aif_greyscale_plan* plan, uint8_t* const data[3], const int pitch[3], int width, int height,
                      int packed, int components);
int aif_greyscale_yuy2(uint8_t* data, int pitch, int width, int height, uint32_t cpu);
int aif_greyscale_chroma(uint8_t* data, int pitch, int width, int height, int bits, uint32_t cpu);
#ifdef __cplusplus
}
#endif
