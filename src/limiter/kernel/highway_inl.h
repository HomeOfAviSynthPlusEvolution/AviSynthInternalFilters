// SPDX-License-Identifier: GPL-2.0-or-later
#include <type_traits>
HWY_BEFORE_NAMESPACE();
namespace aif::limiter {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
template <class T>
void Typed(T* p, int w, const aif_limiter_limits& l, bool chroma) {
  const hn::ScalableTag<T> d;
  auto lo = hn::Set(d, T(chroma ? l.min_chroma : l.min_luma)), hi = hn::Set(d, T(chroma ? l.max_chroma : l.max_luma));
  int x = 0, n = int(hn::Lanes(d));
  for (; x <= w - n; x += n) {
    auto v = hn::LoadU(d, p + x);
    auto result = v;
    if constexpr (std::is_floating_point_v<T>)
      result = hn::IfThenElse(hn::Lt(v, lo), lo, hn::IfThenElse(hn::Gt(v, hi), hi, v));
    else
      result = hn::Min(hn::Max(v, lo), hi);
    hn::StoreU(result, d, p + x);
  }
  scalar(reinterpret_cast<uint8_t*>(p + x), w - x, l, chroma, false);
}
void LimitRow(uint8_t* p, int w, const aif_limiter_limits& l, bool chroma, bool yuy2) {
  if (yuy2) {
    const hn::ScalableTag<uint8_t> d;
    auto lo = hn::Set(d, uint8_t(l.min_luma)), hi = hn::Set(d, uint8_t(l.max_luma));
    auto uvlo = hn::Set(d, uint8_t(l.min_chroma)), uvhi = hn::Set(d, uint8_t(l.max_chroma));
    auto odd = hn::Eq(hn::And(hn::Iota(d, 0), hn::Set(d, uint8_t(1))), hn::Set(d, uint8_t(1)));
    lo = hn::IfThenElse(odd, uvlo, lo);
    hi = hn::IfThenElse(odd, uvhi, hi);
    int x = 0, n = int(hn::Lanes(d));
    for (; x <= 2 * w - n; x += n)
      hn::StoreU(hn::Min(hn::Max(hn::LoadU(d, p + x), lo), hi), d, p + x);
    scalar(p + x, w - x / 2, l, false, true);
    return;
  }
  if (l.bits == 8)
    Typed(p, w, l, chroma);
  else if (l.bits == 32)
    Typed(reinterpret_cast<float*>(p), w, l, chroma);
  else
    Typed(reinterpret_cast<uint16_t*>(p), w, l, chroma);
}
} // namespace HWY_NAMESPACE
} // namespace aif::limiter
HWY_AFTER_NAMESPACE();
