// SPDX-License-Identifier: GPL-2.0-or-later
#include <mask/kernel.h>
#include <vector>
#include <cstdio>
#include <cstdint>
int main() {
  auto ref = aif_mask_resolve(0);
  const auto supported = aif_mask_supported_cpu();
  int cases = 0;
  for (uint32_t cpu : {0u, 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, supported}) {
    if (cpu && (cpu & supported) != cpu)
      continue;
    auto fn = aif_mask_resolve(cpu);
    for (int width : {0, 1, 2, 3, 4, 7, 15, 16, 17, 31, 32, 33, 63, 64, 65, 129})
      for (int op = 0; op < 3; ++op)
        for (uint32_t tol : {0u, 0x0a1020u, 0xffffffu}) {
          std::vector<uint8_t> src(width * 4 + 128, 0xA5), a(src.size(), 0xCD), b;
          for (int i = 0; i < width * 4; ++i) {
            src[i + 64] = uint8_t(i * 29 + 13);
            a[i + 64] = uint8_t(i * 37 + 19);
          }
          auto before = src;
          b = a;
          ref(a.data() + 64, src.data() + 64, width, op, 0x806040, tol);
          fn(b.data() + 64, src.data() + 64, width, op, 0x806040, tol);
          if (a != b || src != before) {
            std::fprintf(stderr, "cpu=%u width=%d op=%d\n", cpu, width, op);
            return 1;
          }
          for (int i = 0; i < width; ++i)
            for (int c = 0; c < 3; ++c)
              if (b[64 + i * 4 + c] != uint8_t((i * 4 + c) * 37 + 19))
                return 2;
          for (int i = 0; i < 64; ++i)
            if (b[i] != 0xCD || b[64 + width * 4 + i] != 0xCD)
              return 3;
          ++cases;
        }
  }
  std::printf("%d mask kernel boundary cases passed\n", cases);
}
