#pragma once
#include "chroma.h"
#include <type_traits>
namespace aif::filters::overlay {
template <class T>
void chroma_scalar_rows(uint8_t* dst, const uint8_t* src, int dp, int sp, int width, int height, bool expand,
                        bool vertical) {
  for (int y = 0; y < height; ++y) {
    auto* d = reinterpret_cast<T*>(dst + ptrdiff_t(y) * dp * (expand && vertical ? 2 : 1));
    const auto* s = reinterpret_cast<const T*>(src + ptrdiff_t(y) * sp * (!expand && vertical ? 2 : 1));
    auto* d2 = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(d) + dp);
    const auto* s2 = reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(s) + sp);
    for (int x = 0; x < width; ++x) {
      if (expand) {
        d[2 * x] = d[2 * x + 1] = s[x];
        if (vertical)
          d2[2 * x] = d2[2 * x + 1] = s[x];
      } else if constexpr (std::is_same_v<T, float>)
        d[x] =
            vertical ? ((s[2 * x] + s[2 * x + 1]) + s2[2 * x] + s2[2 * x + 1]) * .25f : (s[2 * x] + s[2 * x + 1]) * .5f;
      else
        d[x] = T(vertical ? (uint32_t(s[2 * x]) + s[2 * x + 1] + s2[2 * x] + s2[2 * x + 1] + 2) >> 2
                          : (uint32_t(s[2 * x]) + s[2 * x + 1] + 1) >> 1);
    }
  }
}
} // namespace aif::filters::overlay
