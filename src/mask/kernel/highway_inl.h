// SPDX-License-Identifier: GPL-2.0-or-later
HWY_BEFORE_NAMESPACE();
namespace aif::mask {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
void MaskRow(uint8_t* dst, const uint8_t* src, int width, int op, uint32_t color, uint32_t tolerance) {
  const hn::ScalableTag<uint32_t> d;
  const int n = int(hn::Lanes(d));
  const auto byte = hn::Set(d, 255u);
  int x = 0;
  for (; x + n <= width; x += n) {
    auto original = hn::LoadU(d, reinterpret_cast<const uint32_t*>(dst + 4 * x));
    auto result = original;
    if (op == 0) {
      auto v = hn::LoadU(d, reinterpret_cast<const uint32_t*>(src + 4 * x));
      auto b = hn::And(v, byte), g = hn::And(hn::ShiftRight<8>(v), byte), r = hn::And(hn::ShiftRight<16>(v), byte);
      auto y = hn::ShiftRight<15>(b * hn::Set(d, 3736u) + g * hn::Set(d, 19234u) + r * hn::Set(d, 9798u) +
                                  hn::Set(d, 16384u));
      result = hn::Or(hn::And(original, hn::Set(d, 0xffffffu)), hn::ShiftLeft<24>(y));
    } else if (op == 2)
      result = hn::Or(hn::And(original, hn::Set(d, 0xffffffu)), hn::Set(d, (color & 255u) << 24));
    else {
      auto b = hn::And(original, byte), g = hn::And(hn::ShiftRight<8>(original), byte),
           r = hn::And(hn::ShiftRight<16>(original), byte);
      auto cb = hn::Set(d, color & 255), cg = hn::Set(d, (color >> 8) & 255), cr = hn::Set(d, (color >> 16) & 255);
      const hn::Rebind<int32_t, decltype(d)> di;
      auto close =
          hn::And(hn::Le(hn::Abs(hn::BitCast(di, b - cb)), hn::Set(di, int(tolerance & 255))),
                  hn::And(hn::Le(hn::Abs(hn::BitCast(di, g - cg)), hn::Set(di, int((tolerance >> 8) & 255))),
                          hn::Le(hn::Abs(hn::BitCast(di, r - cr)), hn::Set(di, int((tolerance >> 16) & 255)))));

      result = hn::AndNot(hn::And(hn::VecFromMask(d, hn::RebindMask(d, close)), hn::Set(d, 0xff000000u)), original);
    }
    hn::StoreU(result, d, reinterpret_cast<uint32_t*>(dst + 4 * x));
  }
  scalar(dst + 4 * x, op == 0 ? src + 4 * x : nullptr, width - x, op, color, tolerance);
}
} // namespace HWY_NAMESPACE
} // namespace aif::mask
HWY_AFTER_NAMESPACE();
