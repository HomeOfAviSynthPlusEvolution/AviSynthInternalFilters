// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "convolution/kernel.h"
namespace aif::convolution {
using Row = void (*)(uint8_t*, const uint8_t* const*, int, const void*, int, int, int, int, float, float);
void scalar(uint8_t*, const uint8_t* const*, int, const void*, int, int, int, int, float, float);
Row backend(uint32_t cpu);
} // namespace aif::convolution
