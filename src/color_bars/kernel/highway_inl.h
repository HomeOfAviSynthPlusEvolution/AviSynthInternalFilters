// SPDX-License-Identifier: GPL-2.0-or-later
HWY_BEFORE_NAMESPACE();
namespace aif::color_bars {
namespace HWY_NAMESPACE {
void CopyRow(uint8_t* dst, const uint8_t* src, int bytes) {
  // Repeated scanline copies measured faster with 256-bit vectors on Zen 4.
  const hwy::HWY_NAMESPACE::CappedTag<uint8_t, 32> d;
  const int n = int(hwy::HWY_NAMESPACE::Lanes(d));
  int x = 0;
  for (; x <= bytes - n; x += n)
    hwy::HWY_NAMESPACE::StoreU(hwy::HWY_NAMESPACE::LoadU(d, src + x), d, dst + x);
  for (; x < bytes; ++x)
    dst[x] = src[x];
}
void PackYuy2(uint8_t* dst, const uint8_t* y, const uint8_t* u, const uint8_t* v, int width) {
  namespace hn = hwy::HWY_NAMESPACE;
  const hn::ScalableTag<uint8_t> d;
  const int n = int(hn::Lanes(d));
  int x = 0;
  for (; x <= width / 2 - n; x += n) {
    hn::VFromD<decltype(d)> y0, y1;
    hn::LoadInterleaved2(d, y + 2 * x, y0, y1);
    hn::StoreInterleaved4(y0, hn::LoadU(d, u + x), y1, hn::LoadU(d, v + x), d, dst + 4 * x);
  }
  for (; x < width / 2; ++x) {
    dst[4 * x] = y[2 * x];
    dst[4 * x + 1] = u[x];
    dst[4 * x + 2] = y[2 * x + 1];
    dst[4 * x + 3] = v[x];
  }
}
const Kernels* GetKernels() {
  static const Kernels kernels{CopyRow, PackYuy2};
  return &kernels;
}
} // namespace HWY_NAMESPACE
} // namespace aif::color_bars
HWY_AFTER_NAMESPACE();
