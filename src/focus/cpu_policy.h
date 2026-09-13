// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "focus/kernel.h"
#include <avs/cpuid.h>
namespace aif::filters::focus {
inline uint32_t host_cpu_mask(int64_t flags, bool arm) {
  uint32_t result = 0;
  if (arm) {
    if (flags & CPUF_ARM_NEON) {
      result |= AIF_FOCUS_NEON;
      if (flags & CPUF_ARM_SVE2)
        result |= AIF_FOCUS_SVE2;
    }
  } else {
    if (flags & CPUF_SSE2)
      result |= AIF_FOCUS_SSE2;
    if (flags & CPUF_SSSE3)
      result |= AIF_FOCUS_SSSE3;
    if (flags & CPUF_SSE4_1)
      result |= AIF_FOCUS_SSE41;
    if (flags & CPUF_AVX2)
      result |= AIF_FOCUS_AVX2;
  }
  return result;
}
} // namespace aif::filters::focus
