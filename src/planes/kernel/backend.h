// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "planes/kernel.h"
namespace aif::planes {
using Row = void (*)(int, const uint8_t*, const uint8_t*, const uint8_t*, uint8_t*, int, int);
void scalar(int, const uint8_t*, const uint8_t*, const uint8_t*, uint8_t*, int, int);
Row backend(uint32_t);
} // namespace aif::planes
