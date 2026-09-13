// SPDX-License-Identifier: GPL-2.0-or-later
#include "invert/kernel.h"
#include <vector>
#include <cstring>
#include <cstdio>
int main() {
  for (uint32_t cpu : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
    if (cpu != ~0u && !(cpu & aif_invert_supported_cpu()))
      continue;
    for (int size : {1, 2, 3, 4, 6, 8})
      for (int w : {1, 2, 7, 15, 16, 17, 31, 32, 33, 63, 64, 65})
        for (int mode : {-1, 0, 1}) {
          if (mode >= 0 && size != 4)
            continue;
          int row = w * size, pitch = (row + 31) / 8 * 8, h = 3;
          std::vector<uint64_t> a(pitch * h / 8), b;
          auto* d = reinterpret_cast<uint8_t*>(a.data());
          for (int i = 0; i < pitch * h; ++i)
            d[i] = uint8_t(i * 31 + 17);
          if (mode >= 0)
            for (int y = 0; y < h; ++y)
              for (int x = 0; x < w; ++x) {
                float v = x == 0 ? 0.0f : x == 1 ? -0.0f : float(x * 37 % 251) / 200 - .1f;
                std::memcpy(d + y * pitch + x * 4, &v, 4);
              }
          auto before = a;
          b = a;
          uint8_t mask[] = {255, 0, 255, 0, 255, 255, 0, 255};
          if (aif_invert_apply(d, pitch, row, h, mask, size, mode, 0) ||
              aif_invert_apply(reinterpret_cast<uint8_t*>(b.data()), pitch, row, h, mask, size, mode, cpu) || a != b)
            return 1;
          for (int y = 0; y < h; ++y)
            if (std::memcmp(d + y * pitch + row, reinterpret_cast<const uint8_t*>(before.data()) + y * pitch + row,
                            pitch - row))
              return 2;
        }
  }
  std::puts("invert scalar/Highway and padding passed");
}
