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

// Avisynth filter: Layer
// by "poptones" (poptones@myrealbox.com)

#pragma once
#include <algorithm>
#include <array>
#include <type_traits>
constexpr std::array<BYTE, 513> difference_table() {
  std::array<BYTE, 513> t{};
  for (int i = 0; i <= 512; ++i)
    t[i] = static_cast<BYTE>(std::clamp(i - 129, 0, 255));
  return t;
}
inline float c8tof(int x) {
  return x / 255.0f;
}
inline float uv8tof(int x) {
  return (x - 128) / 255.0f;
}
template <typename pixel_t, int midpixel, bool chroma>
static void subtract_plane(BYTE* src1p, const BYTE* src2p, int src1_pitch, int src2_pitch, int width, int height,
                           int bits_per_pixel) {
  typedef typename std::conditional<sizeof(pixel_t) == 4, float, int>::type limits_t;

  const limits_t limit_lo = sizeof(pixel_t) <= 2 ? 0 : (limits_t)(chroma ? uv8tof(0) : c8tof(0));
  const limits_t limit_hi = sizeof(pixel_t) == 1   ? 255
                            : sizeof(pixel_t) == 2 ? ((1 << bits_per_pixel) - 1)
                                                   : (limits_t)(chroma ? uv8tof(255) : c8tof(255));
  const limits_t equal_luma = sizeof(pixel_t) == 1   ? midpixel
                              : sizeof(pixel_t) == 2 ? (midpixel << (bits_per_pixel - 8))
                                                     : (limits_t)(chroma ? uv8tof(midpixel) : c8tof(midpixel));
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      reinterpret_cast<pixel_t*>(src1p)[x] =
          (pixel_t)clamp((limits_t)(reinterpret_cast<pixel_t*>(src1p)[x] - reinterpret_cast<const pixel_t*>(src2p)[x] +
                                    equal_luma), // 126: luma of equality
                         limit_lo, limit_hi);
    }
    src1p += src1_pitch;
    src2p += src2_pitch;
  }
}
