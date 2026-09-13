// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "invert/kernel.h"
namespace aif::invert {
using Row = void (*)(uint8_t*, int, const uint8_t*, int, int);
void scalar(uint8_t*, int, const uint8_t*, int, int);
Row backend(uint32_t);
} // namespace aif::invert
