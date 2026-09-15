// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <cstdlib>
namespace aif::mask {
void scalar(uint8_t* d, const uint8_t* s, int width, int op, uint32_t color, uint32_t tol) {
  for (int x = 0; x < width; ++x, d += 4) {
    if (op == 0) {
      d[3] = (3736 * s[0] + 19234 * s[1] + 9798 * s[2] + 16384) >> 15;
      s += 4;
    } else if (op == 2)
      d[3] = uint8_t(color);
    else if (std::abs(int(d[0]) - int(color & 255)) <= int(tol & 255) &&
             std::abs(int(d[1]) - int((color >> 8) & 255)) <= int((tol >> 8) & 255) &&
             std::abs(int(d[2]) - int((color >> 16) & 255)) <= int((tol >> 16) & 255))
      d[3] = 0;
  }
}
} // namespace aif::mask
