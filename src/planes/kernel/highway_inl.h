// SPDX-License-Identifier: GPL-2.0-or-later
HWY_BEFORE_NAMESPACE();
namespace aif::planes {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
void ProcessRow(int op, const uint8_t* s, const uint8_t* u, const uint8_t* v, uint8_t* dst, int count, int pos) {
#if HWY_ARCH_X86 && HWY_TARGET != HWY_SSE2 && HWY_TARGET != HWY_SCALAR && HWY_TARGET != HWY_EMU128
  if (op == 0) {
    const hn::ScalableTag<uint8_t> d;
    HWY_ALIGN static constexpr uint8_t order[16] = {0, 3, 2, 1, 4, 7, 6, 5, 8, 11, 10, 9, 12, 15, 14, 13};
    const auto indices = hn::LoadDup128(d, order);
    const int pairs = int(hn::Lanes(d)) / 4;
    int x = 0;
    for (; x <= count - pairs; x += pairs) {
      const auto value = hn::TableLookupBytes(hn::LoadU(d, s + 4 * x), indices);
      // Stream complete cache lines only; leave the final partial line cached.
      if (pos == 1 && x + pairs <= (count & ~15))
        hn::Stream(value, d, dst + 4 * x);
      else
        hn::StoreU(value, d, dst + 4 * x);
    }
    scalar(op, s + 4 * x, nullptr, nullptr, dst + 4 * x, count - x, pos);
    return;
  }
#endif
  if (op == 0 || (HWY_TARGET == HWY_SSE2 && op <= 2)) {
    // A YUY2 pair is one 32-bit lane: extract chroma without unpacking all four channels.
    const hn::ScalableTag<uint32_t> d32;
    const hn::Rebind<uint8_t, decltype(d32)> d8;
    const int n = int(hn::Lanes(d32));
    int x = 0;
    for (; x <= count - n; x += n) {
      const auto pair = hn::LoadU(d32, reinterpret_cast<const uint32_t*>(s + 4 * x));
      if (op == 0) {
        const auto mask = hn::Set(d32, HWY_IS_LITTLE_ENDIAN ? 0x00ff00ffu : 0xff00ff00u);
        const auto swapped = hn::Or(hn::And(pair, mask), hn::AndNot(mask, hn::RotateRight<16>(pair)));
        hn::StoreU(swapped, d32, reinterpret_cast<uint32_t*>(dst + 4 * x));
      } else {
        const auto shifted = pos == 1 ? hn::ShiftRight<8>(pair) : hn::ShiftRight<24>(pair);
        const auto selected = hn::DemoteTo(d8, hn::And(shifted, hn::Set(d32, 255u)));
        if (op == 1)
          hn::StoreU(selected, d8, dst + x);
        else
          hn::StoreInterleaved2(selected, hn::Set(d8, uint8_t(128)), d8, dst + 2 * x);
      }
    }
    scalar(op, s + 4 * x, nullptr, nullptr, dst + x * (op == 0 ? 4 : op == 1 ? 1 : 2), count - x, pos);
    return;
  }
  if (op <= 2) {
#if HWY_TARGET == HWY_AVX3
    // AVX3's byte deinterleave measured slower at 512 bits; DL/Zen4 stay full width.
    const hn::CappedTag<uint8_t, 32> d;
#else
    const hn::ScalableTag<uint8_t> d;
#endif
    int x = 0, n = int(hn::Lanes(d));
    for (; x <= count - n; x += n) {
      auto a = hn::Zero(d), b = a, c = a, e = a;
      hn::LoadInterleaved4(d, s + 4 * x, a, b, c, e);
      const auto selected = pos == 1 ? b : e;
      if (op == 1)
        hn::StoreU(selected, d, dst + x);
      else
        hn::StoreInterleaved2(selected, hn::Set(d, uint8_t(128)), d, dst + 2 * x);
    }
    scalar(op, s + 4 * x, nullptr, nullptr, dst + x * (op == 1 ? 1 : 2), count - x, pos);
    return;
  }
  const hn::ScalableTag<uint8_t> d;
  int x = 0, n = int(hn::Lanes(d));
  for (; x <= count - n; x += n) {
    auto b = hn::Zero(d), c = b, e = b;
    hn::LoadInterleaved2(d, u + 2 * x, b, e);
    hn::LoadInterleaved2(d, v + 2 * x, c, e);

    // Fetch both luma channels independently; chroma of the Y clip is discarded.
    auto y0 = hn::Set(d, uint8_t(126)), y1 = y0;
    if (s) {
      auto ignored = hn::Zero(d);
      hn::LoadInterleaved4(d, s + 4 * x, y0, ignored, y1, e);
    }
    hn::StoreInterleaved4(y0, b, y1, c, d, dst + 4 * x);
  }
  scalar(op, s ? s + 4 * x : nullptr, u ? u + 2 * x : nullptr, v ? v + 2 * x : nullptr,
         dst + (op == 1   ? x
                : op == 2 ? 2 * x
                          : 4 * x),
         count - x, pos);
}
} // namespace HWY_NAMESPACE
} // namespace aif::planes
HWY_AFTER_NAMESPACE();
