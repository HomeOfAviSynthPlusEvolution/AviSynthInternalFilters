#include <mask/kernel.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
int main() {
  constexpr int w = 960, h = 540;
  std::vector<uint8_t> a(w * h * 4), b(a.size());
  for (size_t i = 0; i < a.size(); ++i) {
    a[i] = uint8_t(i * 37);
    b[i] = uint8_t(i * 19);
  }
  auto supported = aif_mask_supported_cpu();
  std::puts("operation,cpu_mask,median_ms");
  for (int op = 0; op < 3; ++op)
    for (uint32_t cpu : {0u, 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, supported}) {
      if (cpu && (cpu & supported) != cpu)
        continue;
      auto fn = aif_mask_resolve(cpu);
      std::vector<double> times;
      for (int i = 0; i < 25; ++i) {
        auto begin = std::chrono::steady_clock::now();
        for (int j = 0; j < 8; ++j)
          for (int y = 0; y < h; ++y)
            fn(a.data() + y * w * 4, b.data() + y * w * 4, w, op, 0x806040, 0x101010);
        auto end = std::chrono::steady_clock::now();
        if (i >= 4)
          times.push_back(std::chrono::duration<double, std::milli>(end - begin).count() / 8);
      }
      std::sort(times.begin(), times.end());
      std::printf("%d,%u,%.6f\n", op, cpu, times[times.size() / 2]);
    }
}
