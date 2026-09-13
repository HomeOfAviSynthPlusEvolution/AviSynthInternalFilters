// SPDX-License-Identifier: GPL-2.0-or-later
#include "convolution/kernel.h"
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
  std::puts("width,height,bits,dim,target,median_us");
  for (int w : {640, 1920})
    for (int bits : {8, 16, 32})
      for (int op : {3, 5, 7, 9}) {
        const int bytes = bits == 8 ? 1 : bits == 32 ? 4 : 2;
        const int h = w == 640 ? 360 : 1080, pitch = w * bytes;
        std::vector<uint8_t> src(size_t(pitch) * h);
        for (size_t i = 0; i < src.size() / bytes; ++i) {
          unsigned v = unsigned(i * 37 + 13) & 255;
          if (bytes == 1)
            src[i] = uint8_t(v);
          else if (bytes == 2) {
            uint16_t a = uint16_t(v * 257);
            std::memcpy(src.data() + i * 2, &a, 2);
          } else {
            float a = float(v) / 255;
            std::memcpy(src.data() + i * 4, &a, 4);
          }
        }
        std::vector<int32_t> matrix(op * op, 1);
        std::vector<float> mf(op * op, 1.f);
        std::vector<uint8_t> expected(size_t(pitch) * h + 1, 0xAD), d(expected);
        auto* expected_ptr = expected.data();
        auto* actual_ptr = d.data();
        auto apply = [&](uint8_t* dst, uint32_t cpu) {
          return aif_convolution_apply(dst, pitch, src.data(), pitch, w, h,
                                       bits == 32 ? (const void*)mf.data() : matrix.data(), op, bits,
                                       (1 << 20) / (op * op), 0, 1.f / (op * op), 0, cpu);
        };
        if (apply(expected_ptr, 0))
          return 1;
        struct Run {
          Target target;
          std::array<double, 7> times;
        };
        std::vector<Run> runs;
        for (const auto& t : targets) {
          if (t.mask && t.mask != ~0u && !(t.mask & aif_convolution_supported_cpu()))
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
            for (int i = 0; i < 1; ++i)
              if (apply(actual_ptr, run.target.mask))
                return 3;
            run.times[round] =
                std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
          }
        for (auto& run : runs) {
          std::sort(run.times.begin(), run.times.end());
          std::printf("%d,%d,%d,%d,%s,%.3f\n", w, h, bits, op, run.target.name, run.times[3]);
        }
      }
}
