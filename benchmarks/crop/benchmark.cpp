// SPDX-License-Identifier: GPL-2.0-or-later
#include "crop/kernel.h"
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
  std::puts("width,height,bytes,border,target,median_us");
  for (int w : {640, 1920})
    for (int bytes : {1, 2, 3, 4, 6, 8})
      for (int op : {-1, 1, 64}) {
        const int h = w == 640 ? 360 : 1080;
        const int border = op < 0 ? 0 : op;
        const int sw = op < 0 ? 1 : w, sh = op < 0 ? 1 : h;
        const int sp = sw * bytes, dr = (w + 2 * border) * bytes, dp = (dr + 63) & ~63;
        const int dh = h + 2 * border;
        const uint8_t pattern[8] = {16, 23, 69, 171, 22, 37, 61, 255};
        std::vector<uint8_t> s(sp * sh, 7), expected(dp * dh, 0xAD), d(expected);
        auto apply = [&](uint8_t* dst, uint32_t cpu) {
          return aif_crop_add_borders(s.data(), sp, sp, sh, dst, dp, dr, dh, border * bytes, border, pattern, bytes,
                                      cpu);
        };
        if (apply(expected.data(), 0))
          return 1;
        struct Run {
          Target target;
          std::array<double, 7> times;
        };
        std::vector<Run> runs;
        for (const auto& t : targets) {
          if (t.mask && t.mask != ~0u && !(t.mask & aif_crop_supported_cpu()))
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
          std::printf("%d,%d,%d,%d,%s,%.3f\n", w, h, bytes, op, run.target.name, run.times[3]);
        }
      }
}
