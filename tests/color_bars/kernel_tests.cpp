#include "color_bars/kernel.h"
#include <vector>
#include <cstdio>
int main() {
  for (int layout = 0; layout < 8; ++layout)
    for (int bits : {8, 10, 16, 32})
      for (int w : {4, 5, 8, 16, 28, 31, 32, 33, 63, 64, 65, 68, 126, 128, 130})
        for (int h : {2, 12, 24})
          for (int hd : {0, 1}) {
            if ((layout == 1 || layout == 2 || layout == 7) && w % 2)
              continue;
            if (layout == 3 && w % 4)
              continue;
            if (hd && layout != 0)
              continue;
            if ((layout == 3 || layout == 7) && bits != 8)
              continue;
            if ((layout == 5 || layout == 6) && bits != 8 && bits != 16)
              continue;
            int size = bits == 8 ? 1 : bits == 32 ? 4 : 2, pitch = (w * 8 + 31) / 32 * 32;
            std::vector<uint8_t> a[4], b[4];
            uint8_t *ap[4] = {}, *bp[4] = {};
            int pitches[4] = {pitch, pitch, pitch, pitch};
            for (int p = 0; p < (layout <= 4 ? 4 : 1); ++p) {
              a[p].assign(pitch * h + 32, 0xAD);
              b[p] = a[p];
              ap[p] = a[p].data();
              bp[p] = b[p].data();
            }
            if (aif_color_bars_draw(ap, pitches, w, h, bits, layout, hd, 0))
              return 1;
            for (uint32_t cpu : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
              if (cpu != ~0u && !(cpu & aif_color_bars_supported_cpu()))
                continue;
              for (int p = 0; p < 4; ++p)
                std::fill(b[p].begin(), b[p].end(), 0xAD);
              if (aif_color_bars_draw(bp, pitches, w, h, bits, layout, hd, cpu))
                return 2;
              for (int p = 0; p < 4; ++p)
                if (a[p] != b[p]) {
                  std::printf("mismatch layout=%d bits=%d w=%d h=%d hd=%d plane=%d\n", layout, bits, w, h, hd, p);
                  return 3;
                }
            }
            for (int p = 0; p < (layout <= 4 ? 4 : 1); ++p) {
              int sx = (p == 1 || p == 2) ? (layout == 3 ? 4 : layout == 1 || layout == 2 ? 2 : 1) : 1;
              int sy = (p == 1 || p == 2) && layout == 2 ? 2 : 1;
              int row = w / sx * size * (layout <= 4 ? 1 : layout == 5 ? 3 : layout == 6 ? 4 : 2);
              for (int y = 0; y < h; ++y)
                for (int x = y < h / sy ? row : 0; x < pitch; ++x)
                  if (a[p][y * pitch + x] != 0xAD) {
                    std::printf("padding layout=%d bits=%d w=%d h=%d hd=%d plane=%d x=%d y=%d\n", layout, bits, w, h,
                                hd, p, x, y);
                    return 4;
                  }
              for (int x = pitch * h; x < pitch * h + 32; ++x)
                if (a[p][x] != 0xAD)
                  return 5;
            }
          }
  uint8_t storage[64] = {};
  uint8_t* p[4] = {storage, nullptr, nullptr, nullptr};
  int pitches[4] = {17, 0, 0, 0};
  if (!aif_color_bars_draw(p, pitches, 4, 2, 8, AIF_COLOR_BARS_BGR32, 0, 0))
    return 6;
  pitches[0] = 10;
  if (!aif_color_bars_draw(p, pitches, 4, 2, 8, AIF_COLOR_BARS_YUY2, 0, 0))
    return 7;
  std::puts("color bars kernels passed");
}
