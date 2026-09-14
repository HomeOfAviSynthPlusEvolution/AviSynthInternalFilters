// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstring>
HWY_BEFORE_NAMESPACE();
namespace aif::greyscale {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
template <class T>
void FillTyped(uint8_t* dst, int bytes, const uint8_t* pattern) {
  const hn::ScalableTag<T> d;
  T value;
  std::memcpy(&value, pattern, sizeof(T));
  const auto v = hn::Set(d, value);
  const int step = int(hn::Lanes(d) * sizeof(T));
  int x = 0;
  for (; x <= bytes - step; x += step)
    hn::StoreU(v, d, reinterpret_cast<T*>(dst + x));
  for (; x < bytes; x += sizeof(T))
    std::memcpy(dst + x, &value, sizeof(T));
}
void FillRow(uint8_t* dst, int bytes, const uint8_t* pattern, int size) {
  switch (size) {
    case 1:
      FillTyped<uint8_t>(dst, bytes, pattern);
      break;
    case 2:
      FillTyped<uint16_t>(dst, bytes, pattern);
      break;
    case 4:
      FillTyped<uint32_t>(dst, bytes, pattern);
      break;
    case 8:
      FillTyped<uint64_t>(dst, bytes, pattern);
      break;
    default:
      scalar(dst, bytes, pattern, size);
      break;
  }
}
// Full-range planar F32: keep the matrix's B,G,R multiply/add order,
// and reuse each luma vector for all three output planes.
void FloatRgbRows(uint8_t* const* data, const int* pitch, int width, int height, float kr, float kg, float kb) {
  const hn::ScalableTag<float> d;
  const int n = int(hn::Lanes(d));
  for (int y = 0; y < height; ++y) {
    auto* r = reinterpret_cast<float*>(data[0] + ptrdiff_t(y) * pitch[0]);
    auto* g = reinterpret_cast<float*>(data[1] + ptrdiff_t(y) * pitch[1]);
    auto* b = reinterpret_cast<float*>(data[2] + ptrdiff_t(y) * pitch[2]);
    int x = 0;
    for (; x <= width - n; x += n) {
      auto luma = hn::Add(hn::Mul(hn::LoadU(d, b + x), hn::Set(d, kb)), hn::Mul(hn::LoadU(d, g + x), hn::Set(d, kg)));
      luma = hn::Add(hn::Add(luma, hn::Mul(hn::LoadU(d, r + x), hn::Set(d, kr))), hn::Zero(d));
      hn::StoreU(luma, d, r + x);
      hn::StoreU(luma, d, g + x);
      hn::StoreU(luma, d, b + x);
    }
    for (; x < width; ++x) {
      const float luma = (b[x] * kb + g[x] * kg + r[x] * kr) + 0.f;
      r[x] = g[x] = b[x] = luma;
    }
  }
}
} // namespace HWY_NAMESPACE
} // namespace aif::greyscale
HWY_AFTER_NAMESPACE();
