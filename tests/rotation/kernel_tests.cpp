// SPDX-License-Identifier: GPL-2.0-or-later
#include "rotation/kernel.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <utility>
static void large_turns() {
  for (auto dims : {std::pair{1919, 1080}, std::pair{1920, 1080}, std::pair{1921, 1081}, std::pair{1936, 1079}}) {
    const int w = dims.first, h = dims.second, sp = ((w * 4 + 63) & ~63) + 4;
    std::vector<uint8_t> source(size_t(sp) * h + 65, 0xCD);
    auto* src = source.data() + 1;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w * 4; ++x)
        src[size_t(y) * sp + x] = uint8_t(x * 37 + y * 19);
    const auto before = source;
    for (int mode : {0, 1, 2}) {
      const int dp = ((h * 4 + 63) & ~63) + (mode == 2 ? 4 : 0);
      const size_t size = size_t(dp) * w;
      std::vector<uint8_t> expected(size + 128, 0xAD), actual(size + 128, 0xAD);
      const size_t off = ((64 - (reinterpret_cast<uintptr_t>(actual.data()) & 63)) & 63) + (mode == 1 ? 4 : 0);
      for (int op : {0, 1}) {
        std::fill(expected.begin(), expected.end(), 0xAD);
        if (aif_rotation_apply(src, expected.data() + off, w * 4, h, sp, dp, 4, op, 0))
          throw std::runtime_error("large scalar rejected");
        for (uint32_t cpu : {16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
          if (cpu != ~0u && !(cpu & aif_rotation_supported_cpu()))
            continue;
          std::fill(actual.begin(), actual.end(), 0xAD);
          if (aif_rotation_apply(src, actual.data() + off, w * 4, h, sp, dp, 4, op, cpu) || actual != expected ||
              source != before)
            throw std::runtime_error("large rotation output, padding, or source differs");
        }
      }
    }
  }
}

int main() {
  try {
    large_turns();
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
