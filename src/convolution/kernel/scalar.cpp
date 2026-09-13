// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include "pixel.h"
void aif::convolution::scalar(uint8_t* dst, const uint8_t* const* rows, int w, const void* m, int dim, int bits,
                              int div, int bias, float fd, float fb) {
  for (int x = 0; x < w; ++x) {
    if (bits == 8)
      dst[x] = uint8_t(integer_pixel<uint8_t>(rows, w, x, static_cast<const int32_t*>(m), dim, div, bias, bits));
    else if (bits <= 16)
      reinterpret_cast<uint16_t*>(dst)[x] =
          uint16_t(integer_pixel<uint16_t>(rows, w, x, static_cast<const int32_t*>(m), dim, div, bias, bits));
    else
      reinterpret_cast<float*>(dst)[x] = float_pixel(rows, w, x, static_cast<const float*>(m), dim, fd, fb);
  }
}
