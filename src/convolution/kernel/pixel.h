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

// SPDX-License-Identifier: GPL-2.0-or-later
// Arithmetic order follows AviSynth's GeneralConvolution (GPL-2.0-or-later).
#pragma once
#include <algorithm>
#include <cstdint>
namespace aif::convolution {
inline int normalize(int64_t sum, int div, int bias, int bits) {
  int64_t result = ((sum * div + (1 << 19)) >> 20) + bias;
  return int(std::clamp<int64_t>(result, 0, (1 << bits) - 1));
}
template <class T>
int integer_pixel(const uint8_t* const* rows, int width, int x, const int32_t* matrix, int dim, int div, int bias,
                  int bits) {
  int64_t sum = 0;
  const int radius = dim / 2;
  for (int y = 0; y < dim; ++y) {
    const auto* row = reinterpret_cast<const T*>(rows[y]);
    for (int k = 0; k < dim; ++k)
      sum += int64_t(row[std::clamp(x + k - radius, 0, width - 1)]) * matrix[y * dim + k];
  }
  return normalize(sum, div, bias, bits);
}
inline float float_pixel(const uint8_t* const* rows, int width, int x, const float* matrix, int dim, float div,
                         float bias) {
  float sum = 0;
  const int radius = dim / 2;
  const bool grouped = (dim == 3 || dim == 5) && x >= radius && x < width - radius;
  for (int y = 0; y < dim; ++y) {
    const auto* row = reinterpret_cast<const float*>(rows[y]);
    if (grouped) {
      float line = row[x - radius] * matrix[y * dim];
      for (int k = 1; k < dim; ++k)
        line += row[x + k - radius] * matrix[y * dim + k];
      sum += line;
    } else
      for (int k = 0; k < dim; ++k)
        sum += row[std::clamp(x + k - radius, 0, width - 1)] * matrix[y * dim + k];
  }
  return sum * div + bias;
}
} // namespace aif::convolution
