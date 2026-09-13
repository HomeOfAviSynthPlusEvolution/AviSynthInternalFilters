// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <algorithm>
#include <cstddef>
#include <limits>
extern "C" int aif_convolution_apply(uint8_t* dst, int dp, const uint8_t* src, int sp, int w, int h, const void* matrix,
                                     int dim, int bits, int div, int bias, float fd, float fb, uint32_t cpu) {
  if (w <= 0 || h <= 0 || (dim != 3 && dim != 5 && dim != 7 && dim != 9) ||
      !(bits == 8 || bits == 10 || bits == 12 || bits == 14 || bits == 16 || bits == 32))
    return 1;
  int size = bits == 8 ? 1 : bits == 32 ? 4 : 2;
  if (!dst || !src || dst == src || !matrix || dp < int64_t(w) * size || sp < int64_t(w) * size || dp % size ||
      sp % size || uintptr_t(dst) % size || uintptr_t(src) % size || uintptr_t(matrix) % 4)
    return 1;
  auto fn = aif::convolution::backend(cpu);
  if (bits <= 16) {
    uint64_t weights = 0;
    const auto* m = static_cast<const int32_t*>(matrix);
    for (int i = 0; i < dim * dim; ++i)
      weights += m[i] < 0 ? uint64_t(-int64_t(m[i])) : uint64_t(m[i]);
    // Include stored high bits when checking arithmetic safety, without clamping samples.
    uint64_t bound = weights * (size == 1 ? 255 : 65535), factor = div < 0 ? uint64_t(-int64_t(div)) : uint64_t(div);
    if (factor && bound > uint64_t(INT64_MAX - (1 << 19)) / factor)
      return 1;
    if (bound > INT32_MAX)
      fn = nullptr;
  }
  if (!fn)
    fn = aif::convolution::scalar;
  for (int y = 0; y < h; ++y) {
    const uint8_t* rows[9];
    for (int k = 0; k < dim; ++k)
      rows[k] = src + ptrdiff_t(std::clamp<int64_t>(int64_t(y) + k - dim / 2, 0, h - 1)) * sp;
    fn(dst + ptrdiff_t(y) * dp, rows, w, matrix, dim, bits, div, bias, fd, fb);
  }
  return 0;
}
