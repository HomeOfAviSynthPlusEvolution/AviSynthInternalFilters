// SPDX-License-Identifier: GPL-2.0-or-later
#include "planes/kernel.h"
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
  for (int w : {640, 1920})
    for (int op : {0, 1, 2, 3, 4}) {
      const int bytes = 1;
      const int h = w == 640 ? 360 : 1080;
      const int row = w * 4, pitch = row;
      std::vector<uint8_t> src(pitch * h, 73), u(pitch * h, 91), v(pitch * h, 113);
      std::vector<uint8_t> expected(pitch * h + 65, 0xAD), d(expected);
      auto aligned = [](std::vector<uint8_t>& v) {
        return reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(v.data()) + 63) & ~uintptr_t(63));
      };
      auto* expected_ptr = aligned(expected);
      auto* actual_ptr = aligned(d);
      auto apply = [&](uint8_t* dst, uint32_t cpu) {
        if (op == 0)
          return aif_planes_swap(src.data(), pitch, dst, pitch, w * 2, h, cpu);
        if (op <= 2)
          return aif_planes_extract_uv(src.data(), pitch, dst, pitch, w, h, 1, op == 2, cpu);
        return aif_planes_assemble(op == 4 ? src.data() : nullptr, pitch, u.data(), pitch, v.data(), pitch, dst, pitch,
                                   w * 2, h, cpu);
      };
      if (apply(expected_ptr, 0))
        return 1;
      struct Run {
        Target target;
        std::array<double, 7> times;
      };
      std::vector<Run> runs;
      for (const auto& t : targets) {
        if (t.mask && t.mask != ~0u && !(t.mask & aif_planes_supported_cpu()))
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
