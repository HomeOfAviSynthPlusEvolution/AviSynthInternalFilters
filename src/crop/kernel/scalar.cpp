// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <cstring>
void aif::crop::scalar(uint8_t* dst, int bytes, const uint8_t* pattern, int size) {
  for (int x = 0; x < bytes; x += size)
    std::memcpy(dst + x, pattern, size);
}
