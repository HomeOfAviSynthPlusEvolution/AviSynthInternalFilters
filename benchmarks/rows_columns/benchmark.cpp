// SPDX-License-Identifier: GPL-2.0-or-later
#include "rows_columns/kernel.h"
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
    for (int bytes : {1, 2, 4, 8})
      for (int period : {2, 3, 4})
        for (int weave : {0, 1}) {
          const int op = period * 10 + weave;
          const int h = w == 960 ? 540 : 1080;
          const int pitch = w * bytes * (weave ? period : 1);
          const int spv = w * bytes * (weave ? 1 : period);
          std::vector<uint8_t> buffers[4];
          const uint8_t* src[4];
          int sp[4];
          for (int c = 0; c < 4; ++c) {
            buffers[c].resize(size_t(spv) * h);
            for (size_t i = 0; i < buffers[c].size(); ++i)
              buffers[c][i] = uint8_t(i * 17 + c * 13);
            src[c] = buffers[c].data();
            sp[c] = spv;
          }
          std::vector<uint8_t> expected(size_t(pitch) * h + 1, 0xAD), d(expected);
          auto* expected_ptr = expected.data();
          auto* actual_ptr = d.data();
          auto apply = [&](uint8_t* dst, uint32_t cpu) {
            return aif_rows_columns_process(src, sp, dst, pitch, w, h, bytes, period, 0, weave, cpu);
          };
          if (apply(expected_ptr, 0))
            return 1;
          struct Run {
            Target target;
            std::array<double, 7> times;
          };
          std::vector<Run> runs;
          for (const auto& t : targets) {
            if (t.mask && t.mask != ~0u && !(t.mask & aif_rows_columns_supported_cpu()))
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
            std::printf("%d,%d,%d,%d,%s,%.3f\n", w, h, bytes, op, run.target.name, run.times[3]);
          }
        }
}
