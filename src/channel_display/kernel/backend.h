// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "channel_display/kernel.h"
namespace aif::channel_display {
using PackedRow = void (*)(const uint8_t*, uint8_t*, int, int, int, int, int);
uint32_t packed_supported_cpu();
PackedRow backend(uint32_t cpu);
} // namespace aif::channel_display
