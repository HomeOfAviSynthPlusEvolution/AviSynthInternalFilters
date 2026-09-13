// SPDX-License-Identifier: GPL-2.0-or-later
HWY_BEFORE_NAMESPACE();
namespace aif::color_adjust {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
template <class T, bool narrow = false>
void Map(uint8_t* dst, const uint8_t* src, int n, const uint32_t* lut, int bits) {
  const hn::CappedTag<uint32_t, narrow ? 8 : HWY_MAX_BYTES / 4> d;
  const hn::Rebind<T, decltype(d)> dt;
  const hn::RebindToSigned<decltype(d)> di;
  auto* out = reinterpret_cast<T*>(dst);
  const auto* in = reinterpret_cast<const T*>(src);
  const int lanes = int(hn::Lanes(d));
  int x = 0;
  for (; x <= n - lanes; x += lanes) {
    auto index = hn::Min(hn::PromoteTo(d, hn::LoadU(dt, in + x)), hn::Set(d, (1u << bits) - 1));
    auto value = hn::GatherIndex(d, lut, hn::BitCast(di, index));
    hn::StoreU(hn::DemoteTo(dt, value), dt, out + x);
  }
  scalar(dst + x * sizeof(T), src + x * sizeof(T), n - x, lut, bits, 1);
}
#if HWY_TARGET == HWY_AVX3_DL || HWY_TARGET == HWY_AVX3_ZEN4 || HWY_TARGET == HWY_AVX3_SPR || HWY_TARGET == HWY_AVX10_2
void MapBytes(uint8_t* dst, const uint8_t* src, int n, const uint32_t* lut) {
  const hn::ScalableTag<uint8_t> d;
  HWY_ALIGN uint8_t table[256];
  for (int i = 0; i < 256; ++i)
    table[i] = uint8_t(lut[i]);
  const auto t0 = hn::LoadU(d, table), t1 = hn::LoadU(d, table + 64), t2 = hn::LoadU(d, table + 128),
             t3 = hn::LoadU(d, table + 192);
  int x = 0;
  for (; x <= n - 64; x += 64) {
    const auto index = hn::LoadU(d, src + x);
    const auto indices = hn::IndicesFromVec(d, hn::And(index, hn::Set(d, uint8_t(63))));
    const auto lo = hn::IfThenElse(hn::Lt(index, hn::Set(d, uint8_t(64))), hn::TableLookupLanes(t0, indices),
                                   hn::TableLookupLanes(t1, indices));
    const auto hi = hn::IfThenElse(hn::Lt(index, hn::Set(d, uint8_t(192))), hn::TableLookupLanes(t2, indices),
                                   hn::TableLookupLanes(t3, indices));
    hn::StoreU(hn::IfThenElse(hn::Lt(index, hn::Set(d, uint8_t(128))), lo, hi), d, dst + x);
  }
  scalar(dst + x, src + x, n - x, lut, 8, 1);
}
#endif
void MapRow(uint8_t* dst, const uint8_t* src, int n, const uint32_t* lut, int bits, int step) {
  if (step != 1) {
    scalar(dst, src, n, lut, bits, step);
    return;
  }
  if (bits == 8) {
#if HWY_TARGET == HWY_AVX3_DL || HWY_TARGET == HWY_AVX3_ZEN4 || HWY_TARGET == HWY_AVX3_SPR || HWY_TARGET == HWY_AVX10_2
    MapBytes(dst, src, n, lut);
#elif HWY_TARGET == HWY_SSE2 || HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4 || HWY_TARGET == HWY_AVX2 ||         \
    HWY_TARGET == HWY_AVX3
    scalar(dst, src, n, lut, bits, step);
#else
    Map<uint8_t>(dst, src, n, lut, bits);
#endif
  } else if (bits == 10 || bits == 12) {
#if HWY_TARGET == HWY_SSE2 || HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
    scalar(dst, src, n, lut, bits, step);
#else
    Map<uint16_t, true>(dst, src, n, lut, bits);
#endif
  } else
    Map<uint16_t>(dst, src, n, lut, bits);
}
} // namespace HWY_NAMESPACE
} // namespace aif::color_adjust
HWY_AFTER_NAMESPACE();
