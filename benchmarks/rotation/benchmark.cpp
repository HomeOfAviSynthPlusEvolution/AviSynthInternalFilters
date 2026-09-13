// SPDX-License-Identifier: GPL-2.0-or-later
#include "rotation/kernel.h"
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
  std::puts("width,height,bytes,operation,target,median_us");
  for (int w : {640, 1920})
    for (int bytes : {1, 2, 4, 8})
      for (int op = 0; op < 5; ++op) {
        const int h = w == 640 ? 360 : 1080;
        const int sp = w * bytes, dp = ((op < 2 ? h : w) * bytes + 63) & ~63;
        std::vector<uint8_t> s(sp * h), expected(dp * (op < 2 ? w : h), 0xAD), d(expected);
        for (size_t i = 0; i < s.size(); ++i)
          s[i] = uint8_t(i * 37 + i / sp);
        if (aif_rotation_apply(s.data(), expected.data(), sp, h, sp, dp, bytes, op, 0))
          return 1;
        struct Run {
          Target target;
          std::array<double, 7> times;
        };
        std::vector<Run> runs;
        for (const auto& t : targets) {
          if (t.mask && t.mask != ~0u && !(t.mask & aif_rotation_supported_cpu()))
            continue;
          if (aif_rotation_apply(s.data(), d.data(), sp, h, sp, dp, bytes, op, t.mask) || d != expected)
            return 2;
          runs.push_back({t, {}});
        }
        // Rotate target order each round to reduce systematic thermal/order bias.
        for (size_t round = 0; round < 7; ++round)
          for (size_t j = 0; j < runs.size(); ++j) {
            auto& run = runs[(j + round) % runs.size()];
            for (int warmup = 0; warmup < 2; ++warmup)
              if (aif_rotation_apply(s.data(), d.data(), sp, h, sp, dp, bytes, op, run.target.mask))
                return 3;
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < 4; ++i)
              if (aif_rotation_apply(s.data(), d.data(), sp, h, sp, dp, bytes, op, run.target.mask))
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
