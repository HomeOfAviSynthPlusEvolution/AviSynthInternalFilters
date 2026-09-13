// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "focus/kernel.h"
namespace aif::focus {
// These gates describe the previous x86 algorithms, not Highway vector widths.
// ARM uses the pre-existing scalar arithmetic at every width.
inline bool vertical_quantized(uint32_t cpu, int row, int bits) {
  const uint32_t sse = AIF_FOCUS_SSE2 | (bits > 8 ? AIF_FOCUS_SSE41 : 0);
  return bits != 32 && (((cpu & AIF_FOCUS_AVX2) && row >= 32) || ((cpu & sse) && row >= 16));
}
inline int horizontal_quantized_prefix(uint32_t cpu, int row, int bits, int layout) {
  if (bits == 32 || layout == AIF_FOCUS_RGB3)
    return 0;
  const uint32_t sse = AIF_FOCUS_SSE2 | (bits > 8 ? AIF_FOCUS_SSE41 : 0);
  if (layout == AIF_FOCUS_PLANAR) {
    const int block = (cpu & AIF_FOCUS_AVX2) && row > 32 ? 32 : (cpu & sse) && row > 16 ? 16 : 0;
    return block ? row / block * block : 0;
  }
  return (cpu & sse) && row > 16 ? row : 0;
}
inline bool temporal_reciprocal16(uint32_t cpu, int row, int layout) {
  return row >= 16 && (cpu & AIF_FOCUS_SSE2) && (layout == AIF_FOCUS_YUY2 || !(cpu & AIF_FOCUS_SSSE3));
}
} // namespace aif::focus
