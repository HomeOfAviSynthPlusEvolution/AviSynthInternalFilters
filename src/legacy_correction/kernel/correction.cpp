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

#include "correction.h"
#include <algorithm>
#include <cstring>
namespace aif::filters::legacy_correction {
void darken(uint8_t* dst, int pitch, int width, int height, int vertex, int slope) {
  const int64_t end = std::min<int64_t>(height, int64_t(vertex) - slope / 16 + 1);
  for (int y = 0; y < end; ++y) {
    const int64_t amount = (int64_t(vertex) - y) * 16 / slope;
    auto* row = dst + ptrdiff_t(y) * pitch;
    for (int x = 0; x < width; ++x)
      row[2 * x] = uint8_t(std::max<int64_t>(0, int64_t(row[2 * x]) - amount));
  }
}
void swap_chroma(uint8_t* dst, int pitch, int row, int height) {
  for (int y = 1; y + 1 < height; y += 4) {
    auto* a = dst + ptrdiff_t(y) * pitch;
    auto* b = a + pitch;
    for (int x = 1; x < row; x += 2)
      std::swap(a[x], b[x]);
  }
}
void blend(uint8_t* dst, const uint8_t* src, int dp, int sp, int row, int height, int cutoff) {
  const int64_t first = int64_t(cutoff) - 31;
  for (int y = 0; y < std::min<int64_t>(first, height); ++y)
    std::memcpy(dst + ptrdiff_t(y) * dp, src + ptrdiff_t(y) * sp, row);
  for (int y = int(std::max<int64_t>(0, std::min<int64_t>(first, height))); y < std::min<int64_t>(cutoff, height - 1);
       ++y) {
    auto* a = dst + ptrdiff_t(y) * dp;
    const auto* b = src + ptrdiff_t(y) * sp;
    const int scale = cutoff - y;
    for (int x = 0; x < row; ++x)
      a[x] = uint8_t(a[x] + (((int(b[x]) - a[x]) * scale + 16) >> 5));
  }
}
void skew(uint8_t* dst, const uint8_t* src, int dp, int sp, int dr, int sr, int height) {
  int x = 0;
  for (int y = 0; y < height; ++y) {
    int offset = 0;
    while (offset < sr) {
      if (x == dr) {
        dst += dp;
        x = 0;
      }
      const int count = std::min(sr - offset, dr - x);
      std::memcpy(dst + x, src + ptrdiff_t(y) * sp + offset, count);
      offset += count;
      x += count;
    }
  }
  std::memset(dst + x, 128, dr - x);
}
} // namespace aif::filters::legacy_correction
