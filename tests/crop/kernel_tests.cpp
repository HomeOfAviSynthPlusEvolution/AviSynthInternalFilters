// SPDX-License-Identifier: GPL-2.0-or-later
#include "crop/kernel.h"
#include <vector>
#include <cstdio>
#include <cstring>
int main() {
  const uint8_t p[8] = {3, 19, 91, 244, 66, 71, 199, 255};
  for (int size : {1, 2, 3, 4, 6, 8})
    for (int w : {1, 3, 15, 16, 17, 31, 32, 33, 65})
      for (int h : {1, 3, 9})
        for (int left : {0, 1, 4})
          for (int top : {0, 1, 4}) {
            int sr = w * size, sp = sr + 8, dr = (w + left + 3) * size, dp = dr + 8, dh = h + top + 2;
            std::vector<uint8_t> src(sp * h, 0xAD), expected(dp * dh, 0xEF), actual(expected);
            for (int y = 0; y < h; ++y)
              for (int x = 0; x < sr; ++x)
                src[y * sp + x] = uint8_t(x + y * 17);
            for (int y = 0; y < dh; ++y)
              for (int x = 0; x < dr; ++x)
                expected[y * dp + x] = (y >= top && y < top + h && x >= left * size && x < left * size + sr)
                                           ? src[(y - top) * sp + x - left * size]
                                           : p[x % size];
            for (uint32_t cpu : {0u, 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
              if (cpu && cpu != ~0u && !(cpu & aif_crop_supported_cpu()))
                continue;
              std::fill(actual.begin(), actual.end(), 0xEF);
              if (aif_crop_add_borders(src.data(), sp, sr, h, actual.data(), dp, dr, dh, left * size, top, p, size,
                                       cpu) ||
                  actual != expected)
                return 1;
            }
          }
  uint8_t s[8] = {}, d[8] = {};
  if (!aif_crop_add_borders(s, 4, 4, 1, d, 8, 8, 1, 1, 0, p, 4, 0))
    return 2;
  std::puts("crop kernels passed");
}
