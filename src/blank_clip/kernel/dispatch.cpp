// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
extern "C" int aif_blank_clip_fill(uint8_t* dst, int pitch, int row, int height, const void* pattern, int size,
                                   uint32_t cpu) {
  if (!dst || !pattern || row <= 0 || height <= 0 || pitch < row ||
      (size != 1 && size != 2 && size != 3 && size != 4 && size != 6 && size != 8))
    return 1;
  if (row % size)
    return 1;
  auto fill = aif::blank_clip::backend(cpu);
  if (!fill)
    fill = aif::blank_clip::scalar;
  for (int y = 0; y < height; ++y, dst += pitch)
    fill(dst, row, static_cast<const uint8_t*>(pattern), size);
  return 0;
}
