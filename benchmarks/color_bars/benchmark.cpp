// SPDX-License-Identifier: GPL-2.0-or-later
#include "color_bars/kernel.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <vector>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif
int main() {
#if defined(_WIN32)
  SetThreadAffinityMask(GetCurrentThread(), 4);
  SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif
  struct Target {
    const char* name;
    uint32_t mask;
  };
  const Target targets[] = {{"C", 0},           {"SSE2", 1},       {"SSSE3", 2},     {"NEON", 4},
                            {"SSE4", 8},        {"AVX2", 16},      {"AVX3", 32},     {"AVX3_DL", 64},
                            {"AVX3_ZEN4", 128}, {"AVX3_SPR", 256}, {"AVX10_2", 512}, {"auto", ~0u}};
  std::puts("width,height,bits,layout,hd,target,median_us");
  for (int w : {960, 1920})
    for (int bits : {8, 16, 32})
      for (int layout = 0; layout < 8; ++layout)
        for (int hd : {0, 1}) {
          const int h = w == 960 ? 540 : 1080;
          if (hd && layout != 0)
            continue;
          if ((layout == 3 || layout == 7) && bits != 8)
            continue;
          if ((layout == 5 || layout == 6) && bits == 32)
            continue;
          const int bytes = bits == 8 ? 1 : bits == 32 ? 4 : 2;
          const int pitch = w * bytes * (layout <= 4 ? 1 : layout == 5 ? 3 : layout == 6 ? 4 : 2);
          const int planes = layout <= 4 ? 3 : 1;
          std::vector<uint8_t> expected(pitch * h * planes, 0xAD), d(expected);
          auto apply = [&](uint8_t* dst, uint32_t cpu) {
            uint8_t* ptrs[4] = {dst, planes > 1 ? dst + pitch * h : nullptr, planes > 1 ? dst + 2 * pitch * h : nullptr,
                                nullptr};
            int pitches[4] = {pitch, pitch, pitch, 0};
            return aif_color_bars_draw(ptrs, pitches, w, h, bits, layout, hd, cpu);
          };
          if (apply(expected.data(), 0))
            return 1;
          struct Run {
            Target target;
            std::array<double, 7> times;
          };
          std::vector<Run> runs;
          for (const auto& t : targets) {
            if (t.mask && t.mask != ~0u && !(t.mask & aif_color_bars_supported_cpu()))
              continue;
            if (apply(d.data(), t.mask) || d != expected)
              return 2;
            runs.push_back({t, {}});
          }
          // Rotate target order each round to reduce systematic thermal/order bias.
          for (size_t round = 0; round < 7; ++round)
            for (size_t j = 0; j < runs.size(); ++j) {
              auto& run = runs[(j + round) % runs.size()];
              for (int warmup = 0; warmup < 2; ++warmup)
                if (apply(d.data(), run.target.mask))
                  return 3;
              const auto start = std::chrono::steady_clock::now();
              for (int i = 0; i < 4; ++i)
                if (apply(d.data(), run.target.mask))
                  return 3;
              run.times[round] =
                  std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 4;
            }
          for (auto& run : runs) {
            std::sort(run.times.begin(), run.times.end());
            std::printf("%d,%d,%d,%d,%d,%s,%.3f\n", w, h, bits, layout, hd, run.target.name, run.times[3]);
          }
        }
}
