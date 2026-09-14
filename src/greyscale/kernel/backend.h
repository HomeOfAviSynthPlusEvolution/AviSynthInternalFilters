// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "greyscale/kernel.h"
namespace aif::greyscale {
using Fill = void (*)(uint8_t*, int, const uint8_t*, int);
void scalar(uint8_t*, int, const uint8_t*, int);
using FloatRgb = void (*)(uint8_t* const*, const int*, int, int, float, float, float);
using PackGray = void (*)(uint8_t*, const uint8_t*, int);
struct Backend {
  Fill fill;
  FloatRgb float_rgb;
  PackGray pack_gray;
};
const Backend* backend(uint32_t cpu);
} // namespace aif::greyscale
