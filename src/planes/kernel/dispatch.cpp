// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <blank_clip/kernel.h>
#include <limits>
#include <cstddef>
namespace {
bool geometry(const void* s, int sp, const void* d, int dp, int sr, int dr, int h) {
  return s && d && h > 0 && sr > 0 && dr > 0 && sp >= sr && dp >= dr;
}
aif::planes::Row row(uint32_t cpu) {
  auto fn = aif::planes::backend(cpu);
  return fn ? fn : aif::planes::scalar;
}
} // namespace
extern "C" int aif_planes_swap(const uint8_t* s, int sp, uint8_t* d, int dp, int w, int h, uint32_t cpu) {
  if (w <= 0 || w % 2 || w > std::numeric_limits<int>::max() / 2 || !geometry(s, sp, d, dp, w * 2, w * 2, h))
    return 1;
  auto fn = row(cpu);
  for (int y = 0; y < h; ++y)
    fn(0, s + ptrdiff_t(y) * sp, nullptr, nullptr, d + ptrdiff_t(y) * dp, w / 2, 0);
  return 0;
}
extern "C" int aif_planes_extract_uv(const uint8_t* s, int sp, uint8_t* d, int dp, int w, int h, int v, int packed,
                                     uint32_t cpu) {
  if (w <= 0 || w > std::numeric_limits<int>::max() / 4 || (v != 0 && v != 1) || (packed != 0 && packed != 1) ||
      (packed && w % 2) || !geometry(s, sp, d, dp, w * 4, w * (packed ? 2 : 1), h))
    return 1;
  auto fn = row(cpu);
  for (int y = 0; y < h; ++y)
    fn(packed ? 2 : 1, s + ptrdiff_t(y) * sp, nullptr, nullptr, d + ptrdiff_t(y) * dp, w, v ? 3 : 1);
  return 0;
}
extern "C" int aif_planes_assemble(const uint8_t* s, int sp, const uint8_t* u, int up, const uint8_t* v, int vp,
                                   uint8_t* d, int dp, int w, int h, uint32_t cpu) {
  if (w <= 0 || w % 4 || w > std::numeric_limits<int>::max() / 2 || !geometry(u, up, d, dp, w, w * 2, h) || !v ||
      vp < w || (s && sp < w * 2))
    return 1;
  auto fn = row(cpu);
  for (int y = 0; y < h; ++y)
    fn(3, s ? s + ptrdiff_t(y) * sp : nullptr, u + ptrdiff_t(y) * up, v + ptrdiff_t(y) * vp, d + ptrdiff_t(y) * dp,
       w / 2, 0);
  return 0;
}
extern "C" int aif_planes_fill(uint8_t* d, int p, int r, int h, const void* v, int b, uint32_t cpu) {
  return aif_blank_clip_fill(d, p, r, h, v, b, cpu);
}
