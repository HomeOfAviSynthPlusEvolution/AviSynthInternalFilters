// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "limiter/kernel.h"
namespace aif::limiter {
using Row = void (*)(uint8_t*, int, const aif_limiter_limits&, bool, bool);
void scalar(uint8_t*, int, const aif_limiter_limits&, bool, bool);
Row backend(uint32_t);
bool valid(const aif_limiter_limits*);
} // namespace aif::limiter
