#include "kernel/chroma.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <algorithm>
using namespace aif::filters::overlay;
int main() {
  int cases = 0;
  std::vector<Chroma> targets{chroma_scalar};
  for (int bit = 0; bit < 63; ++bit) {
    auto f = select_chroma(int64_t{1} << bit);
    if (std::find(targets.begin(), targets.end(), f) == targets.end())
      targets.push_back(f);
  }
  for (auto fn : targets)
    for (int bytes : {1, 2, 4})
      for (bool expand : {false, true})
        for (bool vertical : {false, true})
          for (int w : {1, 2, 3, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65, 129}) {
            const int h = 3, sw = w * (expand ? 1 : 2), sh = h * (!expand && vertical ? 2 : 1),
                      dw = w * (expand ? 2 : 1), dh = h * (expand && vertical ? 2 : 1);
            const int sp = sw * bytes + 16, dp = dw * bytes + 16;
            std::vector<uint8_t> src(sp * sh + 64, 0xA7), dst(dp * dh + 64, 0xCD), expected = dst;
            auto* s = src.data() + 32;
            auto* d = dst.data() + 32;
            auto* e = expected.data() + 32;
            for (int y = 0; y < sh; ++y)
              for (int x = 0; x < sw; ++x) {
                const uint32_t value = (x * 193 + y * 73) & 65535;
                if (bytes == 1)
                  s[y * sp + x] = uint8_t(value);
                else if (bytes == 2) {
                  uint16_t v = uint16_t((x % 3) ? value : 65535);
                  std::memcpy(s + y * sp + x * 2, &v, 2);
                } else {
                  float v = float(int(value) - 32768) / 16384;
                  std::memcpy(s + y * sp + x * 4, &v, 4);
                }
              }
            const auto before = src;
            // Independent per-output oracle, including maximum U16 sums and source padding.
            for (int y = 0; y < dh; ++y)
              for (int x = 0; x < dw; ++x) {
                const int sx = expand ? x / 2 : 2 * x, sy = expand ? (vertical ? y / 2 : y) : (vertical ? 2 * y : y);
                const int count = expand ? 1 : (vertical ? 4 : 2);
                double sum = 0;
                float fsum = 0;
                for (int i = 0; i < count; ++i) {
                  const auto* v = s + (sy + (i / 2)) * sp + (sx + i % 2) * bytes;
                  if (bytes == 1)
                    sum += *v;
                  else if (bytes == 2) {
                    uint16_t n;
                    std::memcpy(&n, v, 2);
                    sum += n;
                  } else {
                    float n;
                    std::memcpy(&n, v, 4);
                    fsum += n;
                  }
                }
                if (bytes == 1)
                  e[y * dp + x] = uint8_t((unsigned(sum) + unsigned(count / 2)) / unsigned(count));
                else if (bytes == 2) {
                  uint16_t v = uint16_t((unsigned(sum) + unsigned(count / 2)) / unsigned(count));
                  std::memcpy(e + y * dp + x * 2, &v, 2);
                } else {
                  float v = fsum / float(count);
                  std::memcpy(e + y * dp + x * 4, &v, 4);
                }
              }
            fn(d, s, dp, sp, w, h, bytes, expand, vertical);
            if (dst != expected || src != before) {
              std::fprintf(stderr, "chroma mismatch bytes=%d width=%d expand=%d vertical=%d\n", bytes, w, expand,
                           vertical);
              return 1;
            }
            ++cases;
          }
  // Minimal counterexample for the old two-stage SSE averaging formula.
  for (auto fn : targets) {
    uint8_t s[] = {0, 1, 0, 1}, d = 0;
    fn(&d, s, 1, 2, 1, 1, 1, false, true);
    if (d != 1)
      return 2;
  }
  std::printf("%d exact chroma cases across %zu targets passed\n", cases, targets.size());
}
