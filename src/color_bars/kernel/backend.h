// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "color_bars/kernel.h"
namespace aif::color_bars {
using Copy = void (*)(uint8_t*, const uint8_t*, int);
using Pack = void (*)(uint8_t*, const uint8_t*, const uint8_t*, const uint8_t*, int);
struct Kernels {
  Copy copy;
  Pack pack_yuy2;
};
const Kernels* backend(uint32_t);
void draw(uint8_t* const[4], const int[4], int, int, int, int, int, const Kernels*);
} // namespace aif::color_bars
