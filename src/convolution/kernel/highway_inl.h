// SPDX-License-Identifier: GPL-2.0-or-later
#include "pixel.h"
HWY_BEFORE_NAMESPACE();
namespace aif::convolution {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
template <class T>
void Integer(uint8_t* dst, const uint8_t* const* rows, int w, const int32_t* m, int dim, int bits, int div, int bias) {
  const hn::ScalableTag<int32_t> d;
  const hn::Rebind<T, decltype(d)> dt;
  const int lanes = int(hn::Lanes(d)), r = dim / 2;
  auto* out = reinterpret_cast<T*>(dst);
  int x = 0;
  for (; x < std::min(r, w); ++x)
    out[x] = T(integer_pixel<T>(rows, w, x, m, dim, div, bias, bits));
  for (; x <= w - r - lanes; x += lanes) {
    auto sum = hn::Zero(d);
    for (int y = 0; y < dim; ++y) {
      const auto* row = reinterpret_cast<const T*>(rows[y]);
      for (int k = 0; k < dim; ++k)
        sum = hn::Add(sum, hn::Mul(hn::PromoteTo(d, hn::LoadU(dt, row + x + k - r)), hn::Set(d, m[y * dim + k])));
    }
    HWY_ALIGN int32_t sums[HWY_MAX_BYTES / sizeof(int32_t)];
    hn::Store(sum, d, sums);
    for (int lane = 0; lane < lanes; ++lane)
      out[x + lane] = T(normalize(sums[lane], div, bias, bits));
  }
  for (; x < w; ++x)
    out[x] = T(integer_pixel<T>(rows, w, x, m, dim, div, bias, bits));
}
void ConvolutionRow(uint8_t* dst, const uint8_t* const* rows, int w, const void* matrix, int dim, int bits, int div,
                    int bias, float fd, float fb) {
  if (bits <= 16) {
    if (bits == 8)
      Integer<uint8_t>(dst, rows, w, static_cast<const int32_t*>(matrix), dim, bits, div, bias);
    else
      Integer<uint16_t>(dst, rows, w, static_cast<const int32_t*>(matrix), dim, bits, div, bias);
    return;
  }
  const auto* m = static_cast<const float*>(matrix);
  auto* out = reinterpret_cast<float*>(dst);
  const hn::ScalableTag<float> d;
  const int lanes = int(hn::Lanes(d)), r = dim / 2;
  int x = 0;
  for (; x < std::min(r, w); ++x)
    out[x] = float_pixel(rows, w, x, m, dim, fd, fb);
  for (; x <= w - r - lanes; x += lanes) {
    auto sum = hn::Zero(d);
    for (int y = 0; y < dim; ++y) {
      const auto* row = reinterpret_cast<const float*>(rows[y]);
      if (dim == 3 || dim == 5) {
        auto line = hn::Mul(hn::LoadU(d, row + x - r), hn::Set(d, m[y * dim]));
        for (int k = 1; k < dim; ++k)
          line = hn::Add(line, hn::Mul(hn::LoadU(d, row + x + k - r), hn::Set(d, m[y * dim + k])));
        sum = hn::Add(sum, line);
      } else
        for (int k = 0; k < dim; ++k)
          sum = hn::Add(sum, hn::Mul(hn::LoadU(d, row + x + k - r), hn::Set(d, m[y * dim + k])));
    }
    hn::StoreU(hn::Add(hn::Mul(sum, hn::Set(d, fd)), hn::Set(d, fb)), d, out + x);
  }
  for (; x < w; ++x)
    out[x] = float_pixel(rows, w, x, m, dim, fd, fb);
}
} // namespace HWY_NAMESPACE
} // namespace aif::convolution
HWY_AFTER_NAMESPACE();
