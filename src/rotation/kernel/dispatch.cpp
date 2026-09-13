// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <climits>
extern "C" int aif_rotation_apply(const uint8_t* s, uint8_t* d, int row, int h, int sp, int dp, int b, int op,
                                  uint32_t cpu) {
  if (!s || !d || s == d || row <= 0 || h <= 0 || sp < row || dp <= 0 || op < 0 || op > 4 ||
      !(b == 0 || b == 1 || b == 2 || b == 3 || b == 4 || b == 6 || b == 8))
    return 1;
  const int step = b ? b : 4;
  if (row % step || (b == 0 && op < 2 && h % 2))
    return 1;
  const int out_step = b ? b : 2;
  if (op < 2 ? (h > INT_MAX / out_step || dp < h * out_step) : dp < row)
    return 1;
  auto fn = aif::rotation::backend(cpu);
  (fn ? fn : aif::rotation::scalar)(s, d, row, h, sp, dp, b, op);
  return 0;
}
