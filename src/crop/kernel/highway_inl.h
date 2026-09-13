// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstring>
HWY_BEFORE_NAMESPACE();
namespace aif::crop {
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
} // namespace HWY_NAMESPACE
} // namespace aif::crop
HWY_AFTER_NAMESPACE();
