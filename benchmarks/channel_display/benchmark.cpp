// SPDX-License-Identifier: GPL-2.0-or-later
#include "channel_display/kernel.h"
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
  std::puts("width,height,bytes,layout,target,median_us");
  for (int w : {960, 1920})
    for (int bytes : {1, 2})
      for (int sc : {1, 3, 4})
        for (int dc : {1, 2, 3, 4}) {
          if (bytes == 2 && dc == 2)
            continue;
          const int h = w == 960 ? 540 : 1080, op = sc * 10 + dc;
          const int sp = (w * bytes * sc + 63) & ~63, pitch = (w * bytes * (dc == 1 ? 1 : dc) + 63) & ~63;
          std::vector<uint8_t> src(size_t(sp) * h), expected(size_t(pitch) * h * 4, 0xAD), d(expected);
          for (size_t i = 0; i < src.size(); ++i)
            src[i] = uint8_t(i * 37 + 19);
          auto* expected_ptr = expected.data();
          auto* actual_ptr = d.data();
          auto apply = [&](uint8_t* dst, uint32_t cpu) {
            uint8_t* p[4] = {dst, nullptr, nullptr, nullptr};
            if (dc == 1)
              for (int i = 1; i < 4; ++i)
                p[i] = dst + size_t(pitch) * h * i;
            const int pitches[4] = {pitch, pitch, pitch, pitch};
            return aif_channel_display_render(src.data(), sp, nullptr, 0, p, pitches, w, h, bytes, sc, dc,
                                              sc == 1 ? 0 : 2, cpu);
          };
          if (apply(expected_ptr, 0))
            return 1;
          struct Run {
            Target target;
            std::array<double, 7> times;
          };
          std::vector<Run> runs;
          for (const auto& t : targets) {
            if (t.mask && t.mask != ~0u && !(t.mask & aif_channel_display_supported_cpu()))
              continue;
            std::fill(actual_ptr, actual_ptr + d.size(), 0xAD);
            if (apply(actual_ptr, t.mask) || !std::equal(expected_ptr, expected_ptr + expected.size(), actual_ptr))
              return 2;
            runs.push_back({t, {}});
          }
          // Rotate target order each round to reduce systematic thermal/order bias.
          for (size_t round = 0; round < 7; ++round)
            for (size_t j = 0; j < runs.size(); ++j) {
              auto& run = runs[(j + round) % runs.size()];
              for (int warmup = 0; warmup < 2; ++warmup)
                if (apply(actual_ptr, run.target.mask))
                  return 3;
              const auto start = std::chrono::steady_clock::now();
              for (int i = 0; i < 4; ++i)
                if (apply(actual_ptr, run.target.mask))
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
