#include "blank_clip/kernel.h"
#include <vector>
#include <cstdio>
int main() {
  const uint8_t p[8] = {3, 5, 99, 244, 199, 16, 111, 255};
  for (int size : {1, 2, 3, 4, 6, 8})
    for (int w : {1, 15, 16, 17, 31, 32, 33, 63, 64, 65})
      for (int h : {1, 7})
        for (int offset : {0, 1}) {
          int row = w * size, pitch = row + 8;
          std::vector<uint8_t> d(pitch * h + 1, 0xAE), ref(d);
          for (int y = 0; y < h; ++y)
            for (int x = 0; x < row; ++x)
              ref[offset + y * pitch + x] = p[x % size];
          for (uint32_t cpu : {0u, 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
            if (cpu && cpu != ~0u && !(cpu & aif_blank_clip_supported_cpu()))
              continue;
            std::fill(d.begin(), d.end(), 0xAE);
            if (aif_blank_clip_fill(d.data() + offset, pitch, row, h, p, size, cpu) || d != ref)
              return 1;
          }
        }
  std::puts("blank clip kernels passed");
}
