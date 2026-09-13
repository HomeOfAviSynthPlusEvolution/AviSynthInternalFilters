// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rotation/kernel.h"
namespace aif::rotation {
using Rotate = void (*)(const uint8_t*, uint8_t*, int, int, int, int, int, int);
void scalar(const uint8_t*, uint8_t*, int, int, int, int, int, int);
Rotate backend(uint32_t cpu);
} // namespace aif::rotation
