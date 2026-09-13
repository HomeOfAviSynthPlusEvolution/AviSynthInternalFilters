// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
namespace {
template <class T>
void row(T* p, int count, const aif_limiter_limits& l, bool chroma, bool yuy2) {
  const T low = T(chroma ? l.min_chroma : l.min_luma), high = T(chroma ? l.max_chroma : l.max_luma);
  const T uvlow = T(l.min_chroma), uvhigh = T(l.max_chroma);
  for (int x = 0; x < count; ++x) {
    bool uv = yuy2 && (x % 2 != 0);
    T lo = uv ? uvlow : low, hi = uv ? uvhigh : high;
    if (p[x] < lo)
      p[x] = lo;
    else if (p[x] > hi)
      p[x] = hi;
  }
}
} // namespace
void aif::limiter::scalar(uint8_t* p, int w, const aif_limiter_limits& l, bool c, bool yuy2) {
  if (l.bits == 8)
    row(p, w * (yuy2 ? 2 : 1), l, c, yuy2);
  else if (l.bits == 32)
    row(reinterpret_cast<float*>(p), w, l, c, false);
  else
    row(reinterpret_cast<uint16_t*>(p), w, l, c, false);
}
