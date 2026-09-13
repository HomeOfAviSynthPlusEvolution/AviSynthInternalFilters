// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <cstddef>
extern "C" int aif_invert_apply(uint8_t* data, int pitch, int row, int height, const void* pattern, int size, int mode,
                                uint32_t cpu) {
  if (!data || row <= 0 || height <= 0 || pitch < row || mode < -1 || mode > 1)
    return 1;
  if (mode >= 0) {
    if (size != 4)
      return 1;
  } else if (!pattern || (size != 1 && size != 2 && size != 3 && size != 4 && size != 6 && size != 8))
    return 1;
  int alignment = size == 3 ? 1 : size == 6 ? 2 : size;
  if (row % size || pitch % alignment || reinterpret_cast<uintptr_t>(data) % alignment)
    return 1;
  auto fn = aif::invert::backend(cpu);
  if (!fn)
    fn = aif::invert::scalar;
  for (int y = 0; y < height; ++y)
    fn(data + ptrdiff_t(y) * pitch, row, static_cast<const uint8_t*>(pattern), size, mode);
  return 0;
}
