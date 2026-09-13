// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://avisynth.nl

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

/*
** Turn. version 0.1
** (c) 2003 - Ernst Peché
**
*/

#include "backend.h"
#include <cstring>
#include <cstddef>
namespace aif::rotation {
static void turn_right_yuy2(const uint8_t* srcp, uint8_t* dstp, int src_rowsize, int src_height, int src_pitch,
                            int dst_pitch) {
  dstp += (src_height - 2) * 2;

  for (int y = 0; y < src_height; y += 2) {
    uint8_t* d0 = dstp - y * 2;
    for (int x = 0; x < src_rowsize; x += 4) {
      int u = (srcp[x + 1] + srcp[x + 1 + src_pitch] + 1) / 2;
      int v = (srcp[x + 3] + srcp[x + 3 + src_pitch] + 1) / 2;

      d0[0] = srcp[x + src_pitch];
      d0[1] = u;
      d0[2] = srcp[x];
      d0[3] = v;
      d0 += dst_pitch;

      d0[0] = srcp[x + src_pitch + 2];
      d0[1] = u;
      d0[2] = srcp[x + 2];
      d0[3] = v;
      d0 += dst_pitch;
    }
    srcp += src_pitch * 2;
  }
}

static void turn_left_yuy2(const uint8_t* srcp, uint8_t* dstp, int src_rowsize, int src_height, int src_pitch,
                           int dst_pitch) {
  turn_right_yuy2(srcp + src_pitch * (src_height - 1), dstp + dst_pitch * (src_rowsize / 2 - 1), src_rowsize,
                  src_height, -src_pitch, -dst_pitch);
}

void scalar(const uint8_t* s, uint8_t* d, int row, int h, int sp, int dp, int bytes, int op) {
  if (bytes == 0 && op < 2) {
    if (op == AIF_ROTATION_LEFT)
      turn_left_yuy2(s, d, row, h, sp, dp);
    else
      turn_right_yuy2(s, d, row, h, sp, dp);
    return;
  }
  const int step = bytes == 0 ? 4 : bytes;
  const int width = row / step;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < width; ++x) {
      int dx = x, dy = y;
      if (op == AIF_ROTATION_LEFT) {
        dx = y;
        dy = width - 1 - x;
      }
      if (op == AIF_ROTATION_RIGHT) {
        dx = h - 1 - y;
        dy = x;
      }
      if (op == AIF_ROTATION_180 || op == AIF_ROTATION_HORIZONTAL)
        dx = width - 1 - x;
      if (op == AIF_ROTATION_180 || op == AIF_ROTATION_VERTICAL)
        dy = h - 1 - y;
      const auto* a = s + ptrdiff_t(y) * sp + x * step;
      auto* b = d + ptrdiff_t(dy) * dp + dx * step;
      if (bytes == 0 && op != AIF_ROTATION_VERTICAL) {
        b[0] = a[2];
        b[1] = a[1];
        b[2] = a[0];
        b[3] = a[3];
      } else
        std::memcpy(b, a, step);
    }
}
} // namespace aif::rotation
