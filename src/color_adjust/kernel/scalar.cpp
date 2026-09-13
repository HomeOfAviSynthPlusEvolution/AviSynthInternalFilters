// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <algorithm>
void aif::color_adjust::scalar(uint8_t* dst, const uint8_t* src, int n, const uint32_t* lut, int bits, int step) {
  if (bits == 8)
    for (int x = 0; x < n; ++x)
      dst[x * step] = uint8_t(lut[src[x * step]]);
  else {
    auto* d = reinterpret_cast<uint16_t*>(dst);
    const auto* s = reinterpret_cast<const uint16_t*>(src);
    for (int x = 0; x < n; ++x)
      d[x * step] = uint16_t(lut[std::min<unsigned>(s[x * step], (1u << bits) - 1)]);
  }
}
