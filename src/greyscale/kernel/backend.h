// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "greyscale/kernel.h"
namespace aif::greyscale {
using Fill = void (*)(uint8_t*, int, const uint8_t*, int);
void scalar(uint8_t*, int, const uint8_t*, int);
Fill backend(uint32_t cpu);
} // namespace aif::greyscale
