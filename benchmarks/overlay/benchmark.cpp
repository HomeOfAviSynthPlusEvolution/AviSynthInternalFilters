#include "kernel/chroma.h"
#include <vector>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstring>
using namespace aif::filters::overlay;
int main() {
  std::vector<std::pair<int64_t, Chroma>> targets{{0, chroma_scalar}};
  for (int bit = 0; bit < 63; ++bit) {
    const auto flag = int64_t{1} << bit;
    auto f = select_chroma(flag);
    bool seen = false;
    for (auto t : targets)
      seen |= t.second == f;
    if (!seen)
      targets.emplace_back(flag, f);
  }
  std::printf("target_bit,bytes,expand,vertical,ms\n");
  for (auto target : targets)
    for (int bytes : {1, 2, 4})
      for (bool expand : {false, true})
        for (bool vertical : {false, true}) {
          const int w = 960, h = 540, sp = w * bytes * (expand ? 1 : 2), dp = w * bytes * (expand ? 2 : 1);
          std::vector<uint8_t> s(sp * h * 2, 0), d(dp * h * 2);
          for (size_t i = 0; i < s.size(); ++i)
            s[i] = uint8_t(i * 37);
          if (bytes == 4)
            for (size_t i = 0; i < s.size(); i += 4) {
              float v = float(i % 193) / 192;
              std::memcpy(s.data() + i, &v, 4);
            }
          std::vector<double> times;
          for (int i = 0; i < 11; ++i) {
            auto begin = std::chrono::steady_clock::now();
            for (int n = 0; n < 8; ++n)
              target.second(d.data(), s.data(), dp, sp, w, h, bytes, expand, vertical);
            auto end = std::chrono::steady_clock::now();
            if (i > 1)
              times.push_back(std::chrono::duration<double, std::milli>(end - begin).count() / 8);
          }
          std::sort(times.begin(), times.end());
          std::printf("%lld,%d,%d,%d,%.6f\n", (long long)target.first, bytes, expand, vertical,
                      times[times.size() / 2]);
        }
}
