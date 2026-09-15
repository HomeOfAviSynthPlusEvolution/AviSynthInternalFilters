HWY_BEFORE_NAMESPACE();
namespace aif::filters::overlay {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
template <class T>
void ChromaRows(uint8_t* dst, const uint8_t* src, int dp, int sp, int width, int height, bool expand, bool vertical) {
  using Acc = std::conditional_t<std::is_same_v<T, float>, float, uint32_t>;
  const hn::ScalableTag<Acc> da;
  const hn::Rebind<T, decltype(da)> dt;
  const int n = int(hn::Lanes(dt));
  for (int y = 0; y < height; ++y) {
    auto* d = reinterpret_cast<T*>(dst + ptrdiff_t(y) * dp * (expand && vertical ? 2 : 1));
    const auto* s = reinterpret_cast<const T*>(src + ptrdiff_t(y) * sp * (!expand && vertical ? 2 : 1));
    auto* d2 = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(d) + dp);
    const auto* s2 = reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(s) + sp);
    int x = 0;
    for (; x + n <= width; x += n) {
      if (expand) {
        const auto v = hn::LoadU(dt, s + x);
        hn::StoreInterleaved2(v, v, dt, d + 2 * x);
        if (vertical)
          hn::StoreInterleaved2(v, v, dt, d2 + 2 * x);
      } else {
        hn::Vec<decltype(dt)> a, b, c, e;
        hn::LoadInterleaved2(dt, s + 2 * x, a, b);
        if (vertical)
          hn::LoadInterleaved2(dt, s2 + 2 * x, c, e);
        if constexpr (std::is_same_v<T, float>) {
          auto sum = hn::Add(a, b);
          if (vertical)
            sum = hn::Add(hn::Add(sum, c), e);
          hn::StoreU(hn::Mul(sum, hn::Set(dt, vertical ? .25f : .5f)), dt, d + x);
        } else {
          auto sum = hn::Add(hn::PromoteTo(da, a), hn::PromoteTo(da, b));
          if (vertical)
            sum = hn::Add(hn::Add(sum, hn::PromoteTo(da, c)), hn::PromoteTo(da, e));
          const auto result = vertical ? hn::ShiftRight<2>(hn::Add(sum, hn::Set(da, 2u)))
                                       : hn::ShiftRight<1>(hn::Add(sum, hn::Set(da, 1u)));
          hn::StoreU(hn::DemoteTo(dt, result), dt, d + x);
        }
      }
    }
    if (x < width)
      chroma_scalar_rows<T>(reinterpret_cast<uint8_t*>(d + (expand ? 2 * x : x)),
                            reinterpret_cast<const uint8_t*>(s + (expand ? x : 2 * x)), dp, sp, width - x, 1, expand,
                            vertical);
  }
}
void ChromaRowsDispatch(uint8_t* d, const uint8_t* s, int dp, int sp, int w, int h, int bytes, bool expand,
                        bool vertical) {
  if (bytes == 1)
    ChromaRows<uint8_t>(d, s, dp, sp, w, h, expand, vertical);
  else if (bytes == 2)
    ChromaRows<uint16_t>(d, s, dp, sp, w, h, expand, vertical);
  else
    ChromaRows<float>(d, s, dp, sp, w, h, expand, vertical);
}
} // namespace HWY_NAMESPACE
} // namespace aif::filters::overlay
HWY_AFTER_NAMESPACE();
