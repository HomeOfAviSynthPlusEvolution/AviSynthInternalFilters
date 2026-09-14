// SPDX-License-Identifier: GPL-2.0-or-later
#include "color_adjust/kernel.h"
#include <vector>
#include <cstdio>
#include <algorithm>
template <class T>
bool test(int bits, int width, int step, uint32_t cpu) {
  int pitch = (width * step + 13) * sizeof(T);
  std::vector<T> s(pitch / sizeof(T) * 3, T(0x55)), d = s, ref = s, table(size_t(1) << bits);
  for (size_t i = 0; i < table.size(); ++i)
    table[i] = T((i * 37 + 11) & ((1u << bits) - 1));
  for (size_t i = 0; i < s.size(); ++i)
    s[i] = T(i * 71 + 65500);
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < width; ++x) {
      int i = y * pitch / sizeof(T) + x * step;
      ref[i] = table[std::min<unsigned>(s[i], (1u << bits) - 1)];
    }
  int rc = aif_color_adjust_map((uint8_t*)d.data(), pitch, (uint8_t*)s.data(), pitch, width, 3, table.data(), bits,
                                step, cpu);
  if (rc || d != ref)
    return false;
  auto inplace_ref = s;
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < width; ++x) {
      int i = y * pitch / sizeof(T) + x * step;
      inplace_ref[i] = table[std::min<unsigned>(s[i], (1u << bits) - 1)];
    }
  return !aif_color_adjust_map((uint8_t*)s.data(), pitch, (uint8_t*)s.data(), pitch, width, 3, table.data(), bits, step,
                               cpu) &&
         s == inplace_ref;
}
bool overlapping_table() {
  for (uint32_t cpu : {0u, ~0u}) {
    std::vector<uint16_t> table(65536);
    for (size_t i = 0; i < table.size(); ++i)
      table[i] = uint16_t(65535 - i);
    const uint16_t input[] = {65535, 0, 1};
    uint16_t output[3] = {};
    if (aif_color_adjust_map(reinterpret_cast<uint8_t*>(output), 6, reinterpret_cast<const uint8_t*>(input), 6, 3, 1,
                             table.data(), 16, 1, cpu) ||
        output[0] != 0 || output[1] != 65535 || output[2] != 65534)
      return false;
    if (aif_color_adjust_map(reinterpret_cast<uint8_t*>(table.data()), 6, reinterpret_cast<const uint8_t*>(input), 6, 3,
                             1, table.data(), 16, 1, cpu) ||
        table[0] != 0 || table[1] != 65535 || table[2] != 65534)
      return false;
  }
  return true;
}
int main() {
  if (!overlapping_table())
    return 2;
  int n = 0;
  for (int bits : {8, 10, 12, 14, 16})
    for (int w : {1, 3, 4, 7, 16, 17, 31, 32, 33, 63, 64, 65, 256, 257})
      for (int step : {1, 2, 3, 4})
        for (uint32_t cpu : {0u, 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
          if (cpu && cpu != ~0u && !(cpu & aif_color_adjust_supported_cpu()))
            continue;
          if (!(bits == 8 ? test<uint8_t>(bits, w, step, cpu) : test<uint16_t>(bits, w, step, cpu)))
            return 1;
          ++n;
        }
  std::printf("%d LUT boundary cases passed\n", n);
  return 0;
}
