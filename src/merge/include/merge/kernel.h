// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
  AIF_MERGE_SSE2 = 1,
  AIF_MERGE_SSSE3 = 2,
  AIF_MERGE_NEON = 4,
  AIF_MERGE_SSE4 = 8,
  AIF_MERGE_AVX2 = 16,
  AIF_MERGE_AVX3 = 32,
  AIF_MERGE_AVX3_DL = 64,
  AIF_MERGE_AVX3_ZEN4 = 128,
  AIF_MERGE_AVX3_SPR = 256,
  AIF_MERGE_AVX10_2 = 512
};
uint32_t aif_merge_supported_cpu(void);
/* In-place base, disjoint source or exact alias. Natural sample alignment,
 * positive pitches. bits=8/10/12/14/16/32; step is bytes (sample size, or 2
 * for one YUY2 channel). No padding or other channels touched. Empty image is
 * a no-op. Weight finite [0,1]. Composite's integer SIMD opacity quantization
 * can differ from its scalar result by 1 LSB; float follows Composite rules. */
/* Immutable CPU policy, resolved once per filter. Safe for concurrent calls
 * on disjoint frames. Processing follows the bounds and arithmetic above.
 * The caller owns the plan; destroy accepts null. */
typedef struct aif_merge_plan aif_merge_plan;
int aif_merge_create(uint32_t cpu, aif_merge_plan** out);
void aif_merge_destroy(aif_merge_plan* plan);
int aif_merge_mix_with_plan(const aif_merge_plan* plan, uint8_t* base, const uint8_t* source, int bp, int sp, int width,
                            int height, int bits, int step, double weight);
#ifdef __cplusplus
}
#endif
