// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstring>
#include <type_traits>
HWY_BEFORE_NAMESPACE();
namespace aif::invert {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
template <class T>
void Integer(uint8_t* dst, int bytes, const uint8_t* pattern) {
  // Repeated 1080p tests favor 256-bit vectors for 1/2/4-byte samples.
  using D = std::conditional_t<sizeof(T) == 8, hn::ScalableTag<T>, hn::CappedTag<T, 32 / sizeof(T)>>;
  const D d;
  T mask;
  std::memcpy(&mask, pattern, sizeof(T));
  auto m = hn::Set(d, mask);
  int x = 0, step = int(hn::Lanes(d) * sizeof(T));
  for (; x <= bytes - step; x += step) {
    auto* p = reinterpret_cast<T*>(dst + x);
    hn::StoreU(hn::Xor(hn::LoadU(d, p), m), d, p);
  }
  for (; x < bytes; x += sizeof(T)) {
    T v;
    std::memcpy(&v, dst + x, sizeof(T));
    v ^= mask;
    std::memcpy(dst + x, &v, sizeof(T));
  }
}
void InvertRow(uint8_t* dst, int bytes, const uint8_t* pattern, int size, int mode) {
  if (mode >= 0) {
    const hn::CappedTag<float, 8> d;
    auto max = hn::Set(d, float(mode));
    int x = 0, step = int(hn::Lanes(d)) * 4;
    for (; x <= bytes - step; x += step) {
      auto* p = reinterpret_cast<float*>(dst + x);
      hn::StoreU(hn::Sub(max, hn::LoadU(d, p)), d, p);
    }
    scalar(dst + x, bytes - x, pattern, size, mode);
    return;
  }
  switch (size) {
    case 1:
      Integer<uint8_t>(dst, bytes, pattern);
      break;
    case 2:
      Integer<uint16_t>(dst, bytes, pattern);
      break;
    case 4:
      Integer<uint32_t>(dst, bytes, pattern);
      break;
    case 8:
      Integer<uint64_t>(dst, bytes, pattern);
      break;
    default:
      scalar(dst, bytes, pattern, size, mode);
      break;
  }
}
} // namespace HWY_NAMESPACE
} // namespace aif::invert
HWY_AFTER_NAMESPACE();
