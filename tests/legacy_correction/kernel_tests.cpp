// SPDX-License-Identifier: GPL-2.0-or-later
#include "kernel/correction.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <stdexcept>
using namespace aif::filters::legacy_correction;
void equal(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
  if (a != b)
    throw std::runtime_error("kernel output or padding mismatch");
}
int main() {
  int cases = 0;
  try {
    for (int row : {4, 12, 64, 132})
      for (int height : {1, 2, 3, 4, 7, 9}) {
        const int pitch = row + 13, guard = 31;
        std::vector<uint8_t> original(2 * guard + pitch * height, 0xB7), next = original;
        for (int y = 0; y < height; ++y)
          for (int x = 0; x < row; ++x) {
            original[guard + y * pitch + x] = uint8_t(x * 17 + y * 29);
            next[guard + y * pitch + x] = uint8_t(255 - x * 11 - y * 7);
          }
        auto actual = original, expected = original;
        for (int y = 0; y < height; ++y)
          for (int x = 1; x < row; x += 2) {
            int source = y;
            if (y % 4 == 1 && y + 1 < height)
              source = y + 1;
            else if (y % 4 == 2)
              source = y - 1;
            expected[guard + y * pitch + x] = original[guard + source * pitch + x];
          }
        swap_chroma(actual.data() + guard, pitch, row, height);
        equal(actual, expected);
        ++cases;
        for (int vertex : {INT32_MIN, -2, 0, 3, INT32_MAX})
          for (int slope : {INT32_MIN, -16, -1, 1, 16, INT32_MAX}) {
            actual = original;
            expected = original;
            for (int y = 0; y < height; ++y)
              if (int64_t(y) <= int64_t(vertex) - slope / 16) {
                int64_t amount = (int64_t(vertex) - y) * 16 / slope;
                for (int x = 0; x < row; x += 2)
                  expected[guard + y * pitch + x] =
                      uint8_t(std::max<int64_t>(0, int64_t(original[guard + y * pitch + x]) - amount));
              }
            darken(actual.data() + guard, pitch, row / 2, height, vertex, slope);
            equal(actual, expected);
            ++cases;
          }
        for (int cutoff : {INT32_MIN, -1, 0, 1, 17, 40, INT32_MAX}) {
          actual = original;
          expected = original;
          for (int y = 0; y < height; ++y)
            for (int x = 0; x < row; ++x) {
              auto& dst = expected[guard + y * pitch + x];
              const int a = dst, b = next[guard + y * pitch + x];
              if (y < int64_t(cutoff) - 31)
                dst = uint8_t(b);
              else if (y < cutoff && y < height - 1) {
                const int t = (b - a) * (cutoff - y) + 16;
                const int delta = t >= 0 ? t / 32 : -((-t + 31) / 32);
                dst = uint8_t(a + delta);
              }
            }
          blend(actual.data() + guard, next.data() + guard, pitch, pitch, row, height, cutoff);
          equal(actual, expected);
          ++cases;
        }
        for (int destrow : {1, 7, 16, 137}) {
          const int destheight = (row * height + destrow - 1) / destrow, dp = destrow + 9;
          actual.assign(2 * guard + dp * destheight, 0xC3);
          expected = actual;
          for (int i = 0; i < destheight * destrow; ++i)
            expected[guard + (i / destrow) * dp + i % destrow] =
                i < row * height ? original[guard + (i / row) * pitch + i % row] : 128;
          skew(actual.data() + guard, original.data() + guard, dp, pitch, destrow, row, height);
          equal(actual, expected);
          ++cases;
        }
      }
    std::printf("%d kernel and canary cases passed\n", cases);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "case %d: %s\n", cases, e.what());
    return 1;
  }
}
