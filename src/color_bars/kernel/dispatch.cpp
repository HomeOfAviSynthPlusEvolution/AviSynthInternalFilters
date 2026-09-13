// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <climits>
extern "C" int aif_color_bars_draw(uint8_t* const p[4], const int pitch[4], int w, int h, int bits, int layout, int hd,
                                   uint32_t cpu) {
  if (!p || !pitch || w <= 0 || h <= 0 || w > INT_MAX / 56 || h > INT_MAX / 12 || layout < 0 || layout > 7 ||
      (bits != 8 && bits != 10 && bits != 12 && bits != 14 && bits != 16 && bits != 32) || (hd && layout != 0))
    return 1;
  int size = bits == 8 ? 1 : bits == 32 ? 4 : 2;
  bool planar = layout <= 4;
  if ((layout == 3 || layout == 7) && bits != 8)
    return 1;
  if ((layout == 5 || layout == 6) && bits != 8 && bits != 16)
    return 1;
  int sx = layout == 3 ? 4 : layout == 1 || layout == 2 || layout == 7 ? 2 : 1;
  int sy = layout == 2 ? 2 : 1;
  if (w % sx || h % sy)
    return 1;
  int row = w * size * (planar ? 1 : layout == 5 ? 3 : layout == 6 ? 4 : 2);
  if (!p[0] || pitch[0] < row || pitch[0] % size)
    return 1;
  if ((layout == AIF_COLOR_BARS_BGR32 && pitch[0] % (4 * size)) || (layout == AIF_COLOR_BARS_YUY2 && pitch[0] % 4))
    return 1;
  if (planar) {
    for (int i = 1; i < 3; ++i)
      if (!p[i] || pitch[i] < w / sx * size || pitch[i] % size)
        return 1;
    if (pitch[1] != pitch[2] || (layout == 4 && pitch[0] != pitch[1]))
      return 1;
    if (p[3] && (pitch[3] < w * size || pitch[3] % size))
      return 1;
  }
  aif::color_bars::draw(p, pitch, w, h, bits, layout, hd, aif::color_bars::backend(cpu));
  return 0;
}
