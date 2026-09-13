// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <cstring>
extern "C" int aif_crop_add_borders(const uint8_t* src, int sp, int sr, int sh, uint8_t* dst, int dp, int dr, int dh,
                                    int left, int top, const void* pattern, int size, uint32_t cpu) {
  if (!src || !dst || !pattern || sr <= 0 || sh <= 0 || dr < sr || dh < sh || sp < sr || dp < dr || left < 0 ||
      left > dr - sr || top < 0 || top > dh - sh ||
      (size != 1 && size != 2 && size != 3 && size != 4 && size != 6 && size != 8))
    return 1;
  if (sr % size || dr % size || left % size)
    return 1;
  auto fill = aif::crop::backend(cpu);
  if (!fill)
    fill = aif::crop::scalar;
  const auto* p = static_cast<const uint8_t*>(pattern);
  for (int y = 0; y < dh; ++y, dst += dp) {
    if (y < top || y >= top + sh)
      fill(dst, dr, p, size);
    else {
      fill(dst, left, p, size);
      std::memcpy(dst + left, src, sr);
      fill(dst + left + sr, dr - left - sr, p, size);
      src += sp;
    }
  }
  return 0;
}
