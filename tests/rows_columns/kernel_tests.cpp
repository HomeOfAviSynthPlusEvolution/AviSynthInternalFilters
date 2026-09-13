#include "rows_columns/kernel.h"
#include <vector>
#include <cstdio>
int main() {
  for (int size : {0, 1, 2, 3, 4, 6, 8})
    for (int count : {2, 14, 16, 18, 31, 32, 33, 34, 63, 64, 65, 66})
      for (int period : {1, 2, 3, 4, 5})
        for (int weave : {0, 1}) {
          if (size == 0 && (count & 1))
            continue;
          int bytes = size ? size : 2, spv = count * bytes * (weave ? 1 : period) + 16,
              dp = count * bytes * (weave ? period : 1) + 16, h = 3;
          std::vector<uint8_t> s[5];
          const uint8_t* src[5];
          int sp[5];
          for (int c = 0; c < 5; ++c) {
            s[c].resize(size_t(spv) * h);
            for (size_t i = 0; i < s[c].size(); ++i)
              s[c][i] = uint8_t(i * 17 + c * 13);
            src[c] = s[c].data();
            sp[c] = spv;
          }
          for (int phase = 0; phase < (weave ? 1 : period); ++phase) {
            std::vector<uint8_t> d(size_t(dp) * h, 71), r = d;
            if (aif_rows_columns_process(src, sp, r.data(), dp, count, h, size, period, phase, weave, 0))
              return 1;
            for (uint32_t cpu : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
              if (cpu != ~0u && !(cpu & aif_rows_columns_supported_cpu()))
                continue;
              std::fill(d.begin(), d.end(), 71);
              if (aif_rows_columns_process(src, sp, d.data(), dp, count, h, size, period, phase, weave, cpu))
                return 2;
              if (d != r)
                return 3;
            }
            for (int y = 0; y < h; ++y)
              for (int x = count * bytes * (weave ? period : 1); x < dp; ++x)
                if (d[y * dp + x] != 71)
                  return 4;
          }
        }
  std::puts("columns and padding passed");
}
