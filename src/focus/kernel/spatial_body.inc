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

for (int y = 0; y < height; ++y) {
  const BYTE* line[65]; // better not make diameter bigger than this...
  for (int h = 0; h < diameter; ++h)
    line[h] = &srcp[ptrdiff_t(src_pitch) * clamp(y + h - (diameter >> 1), 0, height - 1)];
  int x;

  int edge = (diameter + 1) & -4;
  for (x = 0; x < std::min(edge, row_size); ++x) // diameter-1 == (diameter>>1) * 2
    dstp[ptrdiff_t(y) * dst_pitch + x] = srcp[ptrdiff_t(y) * src_pitch + x];
  for (; x < row_size - edge; x += 2) {
    int cnt = 0, _y = 0, _u = 0, _v = 0;
    int xx = x | 3;
    int Y = srcp[ptrdiff_t(y) * src_pitch + x], U = srcp[ptrdiff_t(y) * src_pitch + xx - 2],
        V = srcp[ptrdiff_t(y) * src_pitch + xx];
    for (int h = 0; h < diameter; ++h) {
      for (int w = -diameter + 1; w < diameter; w += 2) {
        int xw = (x + w) | 3;
        if (IsClose(line[h][x + w], Y, luma_threshold) && IsClose(line[h][xw - 2], U, chroma_threshold) &&
            IsClose(line[h][xw], V, chroma_threshold)) {
          ++cnt;
          _y += line[h][x + w];
          _u += line[h][xw - 2];
          _v += line[h][xw];
        }
      }
    }
    dstp[ptrdiff_t(y) * dst_pitch + x] = static_cast<uint8_t>((_y + (cnt >> 1)) / cnt);
    if (!(x & 3)) {
      dstp[ptrdiff_t(y) * dst_pitch + x + 1] = static_cast<uint8_t>((_u + (cnt >> 1)) / cnt);
      dstp[ptrdiff_t(y) * dst_pitch + x + 3] = static_cast<uint8_t>((_v + (cnt >> 1)) / cnt);
    }
  }
  for (; x < row_size; ++x)
    dstp[ptrdiff_t(y) * dst_pitch + x] = srcp[ptrdiff_t(y) * src_pitch + x];
}
