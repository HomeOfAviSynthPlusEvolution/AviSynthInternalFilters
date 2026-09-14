#include "rgb_merge/kernel.h"
#include <vector>
#include <cstdio>
bool large_pack() {
  for (int bytes : {1, 2})
    for (int kind : {1, 3}) {
      const int w = 1921, h = 1080, spv = w * bytes * kind + 16, dpv = w * bytes * 4 + 16;
      std::vector<uint8_t> source(size_t(spv) * h), output(size_t(dpv) * h, 71), reference = output;
      for (size_t i = 0; i < source.size(); ++i)
        source[i] = uint8_t(i * 37 + 19);
      const uint8_t* src[] = {source.data(), source.data(), source.data(), nullptr};
      int sp[] = {spv, spv, spv, spv}, kinds[] = {kind, kind, kind, kind}, dp[] = {dpv, dpv, dpv, dpv};
      uint8_t* dst[] = {reference.data(), nullptr, nullptr, nullptr};
      if (aif_rgb_merge_render(src, sp, kinds, dst, dp, w, h, bytes, 4, 0))
        return false;
      for (uint32_t cpu : {uint32_t(AIF_RGB_MERGE_AVX3_ZEN4), ~0u}) {
        if (cpu != ~0u && !(cpu & aif_rgb_merge_supported_cpu()))
          continue;
        dst[0] = output.data();
        if (aif_rgb_merge_render(src, sp, kinds, dst, dp, w, h, bytes, 4, cpu) || output != reference)
          return false;
      }
    }
  return true;
}
int main() {
  if (!large_pack())
    return 5;
  for (uint32_t cpu : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
    if (cpu != ~0u && !(cpu & aif_rgb_merge_supported_cpu()))
      continue;
    for (int bytes : {1, 2, 4})
      for (int w : {1, 2, 16, 18, 31, 32, 33, 34, 63, 64, 65, 66})
        for (int k : {1, 2, 3, 4})
          for (int dc : {1, 3, 4}) {
            if ((bytes == 4 && (k != 1 || dc != 1)) || (k == 2 && (bytes != 1 || w % 2)))
              continue;
            int spv = w * bytes * k + 16, dpv = w * bytes * (dc == 1 ? 1 : dc) + 16, h = 3;
            std::vector<uint8_t> s(size_t(spv) * h);
            for (size_t i = 0; i < s.size(); ++i)
              s[i] = uint8_t(i * 17 + 3);
            const uint8_t* src[] = {s.data(), s.data(), s.data(), k == 3 ? nullptr : s.data()};
            int sp[] = {spv, spv, spv, spv}, kind[] = {k, k, k, k}, dp[] = {dpv, dpv, dpv, dpv};
            std::vector<uint8_t> d[4], r[4];
            uint8_t* p[4];
            for (int c = 0; c < 4; ++c) {
              d[c] = std::vector<uint8_t>(size_t(dpv) * h, 71);
              r[c] = d[c];
              p[c] = dc == 1 || c == 0 ? r[c].data() : nullptr;
            }
            if (aif_rgb_merge_render(src, sp, kind, p, dp, w, h, bytes, dc, 0))
              return 1;
            for (int c = 0; c < 4; ++c)
              p[c] = dc == 1 || c == 0 ? d[c].data() : nullptr;
            if (aif_rgb_merge_render(src, sp, kind, p, dp, w, h, bytes, dc, cpu))
              return 2;
            for (int c = 0; c < 4; ++c)
              if (d[c] != r[c])
                return 3;
            for (int c = 0; c < (dc == 1 ? 4 : 1); ++c)
              for (int y = 0; y < h; ++y)
                for (int x = w * bytes * (dc == 1 ? 1 : dc); x < dpv; ++x)
                  if (d[c][y * dpv + x] != 71)
                    return 4;
          }
  }
  std::puts("RGB merge layout and padding passed");
}
