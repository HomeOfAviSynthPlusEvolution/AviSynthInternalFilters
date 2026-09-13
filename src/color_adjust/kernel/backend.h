// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "color_adjust/kernel.h"
namespace aif::color_adjust {
using Row = void (*)(uint8_t*, const uint8_t*, int, const uint32_t*, int, int);
void scalar(uint8_t*, const uint8_t*, int, const uint32_t*, int, int);
Row backend(uint32_t cpu);
} // namespace aif::color_adjust
