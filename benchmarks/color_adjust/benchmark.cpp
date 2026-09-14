// SPDX-License-Identifier: GPL-2.0-or-later
#include "color_adjust/kernel.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <vector>
#include <cstring>
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
  std::puts("width,height,bits,step,target,median_us");
  for (int w : {960, 1920})
    for (int bits : {8, 10, 12, 14, 16})
      for (int step : {1, 2, 4}) {
        const int bytes = bits == 8 ? 1 : 2, op = step;
        const int h = w == 960 ? 540 : 1080, pitch = w * bytes * step;
        std::vector<uint8_t> src(size_t(pitch) * h);
        std::vector<uint16_t> table(1u << bits);
        for (size_t i = 0; i < table.size(); ++i)
          table[i] = uint16_t((i * 37 + 11) & ((1u << bits) - 1));
        std::vector<uint8_t> table8(table.size());
        for (size_t i = 0; i < table.size(); ++i)
          table8[i] = uint8_t(table[i]);
        for (size_t i = 0; i < src.size() / bytes; ++i) {
          uint16_t v = uint16_t((i * 71 + 13) & ((1u << bits) - 1));
          if (bytes == 1)
            src[i] = uint8_t(v);
          else
            std::memcpy(src.data() + i * 2, &v, 2);
        }
        std::vector<uint8_t> expected(size_t(pitch) * h + 1, 0xAD), d(expected);
        auto* expected_ptr = expected.data();
        auto* actual_ptr = d.data();
        auto apply = [&](uint8_t* dst, uint32_t cpu) {
          return aif_color_adjust_map(dst, pitch, src.data(), pitch, w, h,
                                      bytes == 1 ? (const void*)table8.data() : table.data(), bits, step, cpu);
        };
        if (apply(expected_ptr, 0))
          return 1;
        struct Run {
          Target target;
          std::array<double, 7> times;
        };
        std::vector<Run> runs;
        for (const auto& t : targets) {
          if (t.mask && t.mask != ~0u && !(t.mask & aif_color_adjust_supported_cpu()))
            continue;
          std::fill(actual_ptr, actual_ptr + pitch * h + 1, 0xAD);
          if (apply(actual_ptr, t.mask) || !std::equal(expected_ptr, expected_ptr + pitch * h + 1, actual_ptr))
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
          std::printf("%d,%d,%d,%d,%s,%.3f\n", w, h, bits, op, run.target.name, run.times[3]);
        }
      }
}
