// SPDX-License-Identifier: GPL-2.0-or-later
#include "convolution/kernel.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
template <class T>
bool test(int bits, int w, int h, int dim, uint32_t cpu, bool big) {
  int stride = w + 7;
  std::vector<T> src(stride * h), dst(stride * h, T(29)), ref = dst;
  std::vector<int32_t> m(dim * dim);
  std::vector<float> mf(dim * dim);
  for (size_t i = 0; i < src.size(); ++i) {
    if (bits == 32)
      src[i] = T(int(i % 53) - 19) * T(.017);
    else
      src[i] = T((i * 353 + 139) & ((1u << bits) - 1));
  }
  for (int i = 0; i < dim * dim; ++i) {
    m[i] = (i % 5 - 1) * (big ? 10000 : 1);
    mf[i] = float(i % 5 - 1) * .1f;
  }
  int div = big ? 27 : 123456;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      if (bits != 32) {
        int64_t sum = 0;
        for (int ky = 0; ky < dim; ++ky)
          for (int kx = 0; kx < dim; ++kx)
            sum +=
                int64_t(src[std::clamp(y + ky - dim / 2, 0, h - 1) * stride + std::clamp(x + kx - dim / 2, 0, w - 1)]) *
                m[ky * dim + kx];
        ref[y * stride + x] = T(std::clamp<int64_t>(((sum * div + (1 << 19)) >> 20) - 3, 0, (1 << bits) - 1));
      } else {
        float sum = 0;
        bool grouped = (dim == 3 || dim == 5) && x >= dim / 2 && x < w - dim / 2;
        for (int ky = 0; ky < dim; ++ky) {
          float line = 0;
          for (int kx = 0; kx < dim; ++kx) {
            float term =
                float(src[std::clamp(y + ky - dim / 2, 0, h - 1) * stride + std::clamp(x + kx - dim / 2, 0, w - 1)]) *
                mf[ky * dim + kx];
            if (grouped) {
              if (kx == 0)
                line = term;
              else
                line += term;
            } else
              sum += term;
          }
          if (grouped)
            sum += line;
        }
        ref[y * stride + x] = T(sum * .7f - .2f);
      }
    }
  if (aif_convolution_apply((uint8_t*)dst.data(), stride * sizeof(T), (uint8_t*)src.data(), stride * sizeof(T), w, h,
                            bits == 32 ? (void*)mf.data() : (void*)m.data(), dim, bits, div, -3, .7f, -.2f, cpu))
    return false;
  for (size_t i = 0; i < dst.size(); ++i)
    if (dst[i] != ref[i]) {
      std::printf("bits%d w%d h%d dim%d cpu%u pos%zu got%g ref%g\n", bits, w, h, dim, cpu, i, double(dst[i]),
                  double(ref[i]));
      return false;
    }
  return true;
}
int main() {
  int count = 0;
  for (int bits : {8, 10, 12, 14, 16, 32})
    for (int w : {1, 2, 3, 4, 7, 9, 16, 19, 31, 32, 33, 63, 64, 65})
      for (int h : {1, 2, 7})
        for (int dim : {3, 5, 7, 9})
          for (uint32_t cpu : {0u, 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u})
            for (bool big : {false, true}) {
              if (cpu && cpu != ~0u && !(cpu & aif_convolution_supported_cpu()))
                continue;
              bool ok = bits == 8    ? test<uint8_t>(bits, w, h, dim, cpu, big)
                        : bits == 32 ? test<float>(bits, w, h, dim, cpu, big)
                                     : test<uint16_t>(bits, w, h, dim, cpu, big);
              if (!ok)
                return 1;
              ++count;
            }
  std::printf("%d independent convolution cases passed\n", count);
  return 0;
}
