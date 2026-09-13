// SPDX-License-Identifier: GPL-2.0-or-later
#include "planes/kernel.h"
#include <vector>
#include <cstdio>
int main() {
  for (uint32_t selected : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
    if (selected != ~0u && !(selected & aif_planes_supported_cpu()))
      continue;
    for (int count : {1, 2, 7, 15, 16, 17, 31, 32, 33, 62, 63, 64, 65, 66})
      for (int uv : {0, 1})
        for (int op : {0, 1, 2, 3, 4}) {
          if (op >= 2 && count % 2)
            continue;
          int sp = count * 4 + 16, dp = sp, h = 3;
          std::vector<uint8_t> s(sp * h), u(sp * h), v(sp * h), a(dp * h, 0x71), b = a;
          for (int i = 0; i < sp * h; ++i) {
            s[i] = uint8_t(i * 37);
            u[i] = uint8_t(i * 13 + 19);
            v[i] = uint8_t(i * 23 + 41);
          }
          for (int mode = 0; mode < 2; ++mode) {
            auto& d = mode ? b : a;
            uint32_t cpu = mode ? selected : 0;
            int status;
            if (op == 0)
              status = aif_planes_swap(s.data(), sp, d.data(), dp, count * 2, h, cpu);
            else if (op <= 2)
              status = aif_planes_extract_uv(s.data(), sp, d.data(), dp, count, h, uv, op == 2, cpu);
            else
              status = aif_planes_assemble(op == 4 ? s.data() : nullptr, sp, u.data(), sp, v.data(), sp, d.data(), dp,
                                           count * 2, h, cpu);
            if (status)
              return 1;
          }
          if (a != b)
            return 2;
          int row = op == 1 ? count : op == 2 ? count * 2 : count * 4;
          for (int y = 0; y < h; ++y)
            for (int x = row; x < dp; ++x)
              if (a[y * dp + x] != 0x71)
                return 3;
        }
  }
  std::puts("planes scalar/Highway and tails passed");
}
