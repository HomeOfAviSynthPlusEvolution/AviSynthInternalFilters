// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "focus/kernel.h"
namespace aif::focus {
using VerticalFn = void(uint8_t*, int, int, int, int, int, float, uint8_t*, uint32_t);
using HorizontalFn = void(const uint8_t*, int, uint8_t*, int, int, int, int, int, int, float, uint32_t);
using TemporalFn = void(uint8_t*, const uint8_t* const*, int, int, int, int, unsigned, unsigned, uint32_t);
using SadFn = int64_t(const uint8_t*, const uint8_t*, int, int, int, int, int);
struct Backend {
  VerticalFn* vertical;
  HorizontalFn* horizontal;
  TemporalFn* temporal;
  SadFn* sad;
};
// No global Highway target overrides. Each call supplies its own allowed mask.
const Backend* backend(uint32_t allowed);
#ifdef AIF_FOCUS_SEPARATE_AVX2
namespace N_AVX2 {
VerticalFn Vertical;
HorizontalFn Horizontal;
TemporalFn Temporal;
SadFn Sad;
} // namespace N_AVX2
#endif
} // namespace aif::focus
