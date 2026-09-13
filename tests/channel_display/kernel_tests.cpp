#include "channel_display/kernel.h"
#include <vector>
#include <cstdio>
int main() {
  for (uint32_t cpu : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
    if (cpu != ~0u && !(cpu & aif_channel_display_supported_cpu()))
      continue;
    for (int bytes : {1, 2})
      for (int w : {1, 2, 16, 18, 31, 32, 33, 34, 62, 63, 64, 65, 66})
        for (int sc : {1, 3, 4})
          for (int dc : {1, 2, 3, 4})
            for (int ch = 0; ch < sc; ++ch) {
              if (dc == 2 && (bytes == 2 || w % 2))
                continue;
              int sp = w * bytes * sc + 16, dp = w * bytes * (dc == 1 ? 1 : dc) + 16, h = 3;
              std::vector<uint8_t> s(sp * h), a(sp * h, 127);
              for (size_t i = 0; i < s.size(); ++i)
                s[i] = uint8_t(i * 17 + 3);
              std::vector<uint8_t> d[4], r[4];
              uint8_t* p[4];
              int pitches[] = {dp, dp, dp, dp};
              for (int c = 0; c < 4; ++c) {
                d[c] = std::vector<uint8_t>(size_t(dp) * size_t(h), 71);
                r[c] = d[c];
                p[c] = dc == 1 || c == 0 ? r[c].data() : nullptr;
              }
              if (aif_channel_display_render(s.data(), sp, a.data(), sp, p, pitches, w, h, bytes, sc, dc, ch, 0))
                return 1;
              for (int c = 0; c < 4; ++c)
                p[c] = dc == 1 || c == 0 ? d[c].data() : nullptr;
              if (aif_channel_display_render(s.data(), sp, a.data(), sp, p, pitches, w, h, bytes, sc, dc, ch, cpu))
                return 2;
              for (int c = 0; c < 4; ++c)
                if (d[c] != r[c])
                  return 3;
              for (int c = 0; c < (dc == 1 ? 4 : 1); ++c)
                for (int y = 0; y < h; ++y)
                  for (int x = w * bytes * (dc == 1 ? 1 : dc); x < dp; ++x)
                    if (d[c][y * dp + x] != 71)
                      return 4;
            }
  }
  std::puts("channel layouts and padding passed");
}
