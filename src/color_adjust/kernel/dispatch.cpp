// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <cstddef>
#include <vector>
extern "C" int aif_color_adjust_map(uint8_t* dst, int dp, const uint8_t* src, int sp, int w, int h, const void* lut,
                                    int bits, int step, uint32_t cpu) {
  if (w < 0 || h < 0 || !(bits == 8 || bits == 10 || bits == 12 || bits == 14 || bits == 16) || step < 1 || step > 4)
    return 1;
  if (!w || !h)
    return 0;
  const int size = bits == 8 ? 1 : 2;
  const int64_t row = (int64_t(w - 1) * step + 1) * size;
  if (!dst || !src || !lut || dp < row || sp < row || dp % size || sp % size || uintptr_t(dst) % size ||
      uintptr_t(src) % size || uintptr_t(lut) % size)
    return 1;
  const uint64_t output_span = uint64_t(h - 1) * dp + row;
  const uintptr_t output_address = uintptr_t(dst), table_address = uintptr_t(lut);
  const bool table_overlap = output_address <= table_address
                                 ? uint64_t(table_address - output_address) < output_span
                                 : output_address - table_address < (uint64_t(1) << bits) * size;
  // Direct U16 lookup avoids expanding the 128 KiB table for every frame.
  // Preserve the old snapshot semantics when output overlaps the table.
  if (bits == 16 && !table_overlap) {
    const auto* table = static_cast<const uint16_t*>(lut);
    for (int y = 0; y < h; ++y) {
      const auto* input = reinterpret_cast<const uint16_t*>(src + ptrdiff_t(y) * sp);
      auto* output = reinterpret_cast<uint16_t*>(dst + ptrdiff_t(y) * dp);
      for (int x = 0; x < w; ++x)
        output[ptrdiff_t(x) * step] = table[input[ptrdiff_t(x) * step]];
    }
    return 0;
  }
  try {
    std::vector<uint32_t> table(size_t(1) << bits);
    for (size_t i = 0; i < table.size(); ++i)
      table[i] = size == 1 ? static_cast<const uint8_t*>(lut)[i] : static_cast<const uint16_t*>(lut)[i];
    auto fn = aif::color_adjust::backend(cpu);
    if (!fn)
      fn = aif::color_adjust::scalar;
    for (int y = 0; y < h; ++y)
      fn(dst + ptrdiff_t(y) * dp, src + ptrdiff_t(y) * sp, w, table.data(), bits, step);
  } catch (...) {
    return 2;
  }
  return 0;
}
