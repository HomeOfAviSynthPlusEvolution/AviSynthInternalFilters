// SPDX-License-Identifier: GPL-2.0-or-later
#include "rotation/kernel.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <stdexcept>
int main() {
  try {
    int cases = 0;
    for (int bytes : {0, 1, 2, 3, 4, 6, 8})
      for (int width : {1, 2, 4, 7, 8, 15, 16, 17, 31, 32, 33, 65})
        for (int height : {1, 2, 4, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65})
          for (int op = 0; op < 5; ++op) {
            if (bytes == 0 && ((width % 2) || (op < 2 && height % 2)))
              continue;
            int row = width * (bytes ? bytes : 2), sp = (row + 31) / 32 * 32, dh = op < 2 ? width : height;
            int dr = op < 2 ? height * (bytes ? bytes : 2) : row, dp = (dr + 31) / 32 * 32;
            std::vector<uint8_t> s(sp * height + 32, 0xCD), a(dp * dh + 32, 0xAD), b = a;
            for (int y = 0; y < height; ++y)
              for (int x = 0; x < row; ++x)
                s[y * sp + x] = uint8_t(x * 37 + y * 19);
            const auto before = s;
            if (aif_rotation_apply(s.data(), a.data(), row, height, sp, dp, bytes, op, 0))
              throw std::runtime_error("scalar rejected");
            for (uint32_t cpu : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
              if (cpu != ~0u && !(cpu & aif_rotation_supported_cpu()))
                continue;
              std::fill(b.begin(), b.end(), 0xAD);
              if (aif_rotation_apply(s.data(), b.data(), row, height, sp, dp, bytes, op, cpu) || a != b || s != before)
                throw std::runtime_error("Highway differs or input changed");
              for (int y = 0; y < dh; ++y)
                for (int x = dr; x < dp; ++x)
                  if (b[y * dp + x] != 0xAD)
                    throw std::runtime_error("padding changed");
              for (size_t i = dp * dh; i < b.size(); ++i)
                if (b[i] != 0xAD)
                  throw std::runtime_error("guard changed");
              ++cases;
            }
          }
    std::printf("%d rotation cases passed; available CPU mask %u\n", cases, aif_rotation_supported_cpu());
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
