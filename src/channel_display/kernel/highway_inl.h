// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstring>
#include <type_traits>
HWY_BEFORE_NAMESPACE();
namespace aif::channel_display {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
template <class T, bool Narrow = false>
void Packed(const uint8_t* source, uint8_t* destination, int width, int sc, int dc, int channel) {
  const auto* src = reinterpret_cast<const T*>(source);
  auto* dst = reinterpret_cast<T*>(destination);
  using D = std::conditional_t<Narrow, hn::CappedTag<T, 32 / sizeof(T)>, hn::ScalableTag<T>>;
  const D d;
  const int n = int(hn::Lanes(d));
  int x = 0;
  // SSE2 deinterleave3 loses to the direct scalar loop for RGB48.
  const bool direct = HWY_TARGET == HWY_SSE2 && sizeof(T) == 2 && sc == 3 && dc == 3;
  for (; !direct && x <= width - n; x += n) {
    auto b = hn::Zero(d), g = b, r = b, a = hn::Set(d, T(~T(0)));
    if (sc == 4)
      hn::LoadInterleaved4(d, src + x * sc, b, g, r, a);
    else
      hn::LoadInterleaved3(d, src + x * sc, b, g, r);
    const auto selected = channel == 0 ? b : channel == 1 ? g : channel == 2 ? r : a;
    if (dc == 4)
      hn::StoreInterleaved4(selected, selected, selected, a, d, dst + x * dc);
    else
      hn::StoreInterleaved3(selected, selected, selected, d, dst + x * dc);
  }
  for (; x < width; ++x) {
    const T selected = src[x * sc + channel];
    for (int c = 0; c < 3; ++c)
      dst[x * dc + c] = selected;
    if (dc == 4)
      dst[x * dc + 3] = sc == 4 ? src[x * sc + 3] : T(~T(0));
  }
}
void RenderPacked(const uint8_t* s, uint8_t* d, int w, int bytes, int sc, int dc, int channel) {
  if (bytes == 1) {
    // Full-width experiments favor 256 bits for RGB24 input and AVX3 byte layouts.
    if (sc == 3 || HWY_TARGET == HWY_AVX3)
      Packed<uint8_t, true>(s, d, w, sc, dc, channel);
    else
      Packed<uint8_t>(s, d, w, sc, dc, channel);
  } else if (sc == 3 && dc == 3)
    Packed<uint16_t, true>(s, d, w, sc, dc, channel);
  else
    Packed<uint16_t>(s, d, w, sc, dc, channel);
}
} // namespace HWY_NAMESPACE
} // namespace aif::channel_display
HWY_AFTER_NAMESPACE();
