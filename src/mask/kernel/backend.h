// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "mask/kernel.h"
namespace aif::mask {
using Row = aif_mask_row;
void scalar(uint8_t*, const uint8_t*, int, int, uint32_t, uint32_t);
Row backend(uint32_t);
} // namespace aif::mask
