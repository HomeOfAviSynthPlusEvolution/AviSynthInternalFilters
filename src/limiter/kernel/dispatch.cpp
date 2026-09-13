// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <cmath>
#include <initializer_list>
#include <limits>
#include <cstddef>
bool aif::limiter::valid(const aif_limiter_limits* l) {
  if (!l || (l->bits != 8 && l->bits != 10 && l->bits != 12 && l->bits != 14 && l->bits != 16 && l->bits != 32))
    return false;
  for (float v : {l->min_luma, l->max_luma, l->min_chroma, l->max_chroma}) {
    if (!std::isfinite(v))
      return false;
    if (l->bits != 32 && (v < 0 || v > float((1u << l->bits) - 1) || std::trunc(v) != v))
      return false;
  }
  return l->min_luma <= l->max_luma && l->min_chroma <= l->max_chroma;
}
extern "C" int aif_limiter_apply(uint8_t* data, int pitch, int w, int h, const aif_limiter_limits* l, int c, int yuy2,
                                 uint32_t cpu) {
  if (!aif::limiter::valid(l) || !data || w <= 0 || h <= 0 || (c != 0 && c != 1) || (yuy2 != 0 && yuy2 != 1))
    return 1;
  if (yuy2 && (l->bits != 8 || w % 2))
    return 1;
  int bytes = l->bits == 8 ? 1 : l->bits == 32 ? 4 : 2, mult = bytes * (yuy2 ? 2 : 1);
  if (w > std::numeric_limits<int>::max() / mult || pitch < w * mult || pitch % bytes ||
      reinterpret_cast<uintptr_t>(data) % bytes)
    return 1;
  auto fn = aif::limiter::backend(cpu);
  if (!fn)
    fn = aif::limiter::scalar;
  for (int y = 0; y < h; ++y)
    fn(data + ptrdiff_t(y) * pitch, w, *l, c != 0 && !yuy2, yuy2 != 0);
  return 0;
}
