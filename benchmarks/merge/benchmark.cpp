// SPDX-License-Identifier: GPL-2.0-or-later
#include "merge/kernel.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <vector>
#include <cstring>
#include <cmath>
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
  std::puts("width,height,bits,layout,target,median_us");
  for (int w : {960, 1920})
    for (int bits : {8, 16, 32})
      for (int step : {1, 2})
        for (int weighted : {0, 1}) {
          if (step == 2 && bits != 8)
            continue;
          const int bytes = bits == 8 ? 1 : bits == 32 ? 4 : 2;
          const int op = step * 10 + weighted;
          const double weight = weighted ? .625 : .5;
          const int h = w == 960 ? 540 : 1080;
          const int pitch = w * bytes * step;
          std::vector<uint8_t> src(size_t(pitch) * h), original(src);
          for (size_t i = 0; i < src.size() / bytes; ++i) {
            unsigned v = unsigned(i * 37 + 13) & 255;
            if (bytes == 1) {
              src[i] = uint8_t(v);
              original[i] = uint8_t(255 - v);
            } else if (bytes == 2) {
              uint16_t a = uint16_t(v * 257), b = uint16_t(65535 - a);
              std::memcpy(src.data() + i * 2, &a, 2);
              std::memcpy(original.data() + i * 2, &b, 2);
            } else {
              float a = float(v) / 255, b = 1 - a;
              std::memcpy(src.data() + i * 4, &a, 4);
              std::memcpy(original.data() + i * 4, &b, 4);
            }
          }
          std::vector<uint8_t> expected(original), d(original);
          expected.push_back(0xAD);
          d.push_back(0xAD);
          auto* expected_ptr = expected.data();
          auto* actual_ptr = d.data();
          auto reset = [&] {
            std::copy(original.begin(), original.end(), d.begin());
          };
          auto apply = [&](uint8_t* dst, uint32_t cpu) {
            return aif_merge_mix(dst, src.data(), pitch, pitch, w, h, bits, bytes * step, weight, cpu);
          };
          auto equal = [&] {
            for (int y = 0; y < h; ++y)
              for (int x = 0; x < w; ++x) {
                size_t off = size_t(y) * pitch + x * bytes * step;
                if (bytes == 1) {
                  if (std::abs(int(d[off]) - int(expected[off])) > 1)
                    return false;
                  if (step == 2 && d[off + 1] != original[off + 1])
                    return false;
                } else if (bytes == 2) {
                  uint16_t a, b;
                  std::memcpy(&a, d.data() + off, 2);
                  std::memcpy(&b, expected.data() + off, 2);
                  if (std::abs(int(a) - int(b)) > 1)
                    return false;
                } else {
                  float a, b;
                  std::memcpy(&a, d.data() + off, 4);
                  std::memcpy(&b, expected.data() + off, 4);
                  if (!(std::abs(a - b) <= 1e-6))
                    return false;
                }
              }
            return d.back() == 0xAD;
          };
          if (apply(expected_ptr, 0))
            return 1;
          struct Run {
            Target target;
            std::array<double, 7> times;
          };
          std::vector<Run> runs;
          for (const auto& t : targets) {
            if (t.mask && t.mask != ~0u && !(t.mask & aif_merge_supported_cpu()))
              continue;
            reset();
            if (apply(actual_ptr, t.mask) || !equal())
              return 2;
            runs.push_back({t, {}});
          }
          // Rotate target order each round to reduce systematic thermal/order bias.
          for (size_t round = 0; round < 7; ++round)
            for (size_t j = 0; j < runs.size(); ++j) {
              auto& run = runs[(j + round) % runs.size()];
              for (int warmup = 0; warmup < 2; ++warmup) {
                reset();
                if (apply(actual_ptr, run.target.mask))
                  return 3;
              }
              double elapsed = 0;
              for (int i = 0; i < 4; ++i) {
                reset();
                const auto start = std::chrono::steady_clock::now();
                if (apply(actual_ptr, run.target.mask))
                  return 3;
                elapsed += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
              }
              run.times[round] = elapsed / 4;
            }
          for (auto& run : runs) {
            std::sort(run.times.begin(), run.times.end());
            std::printf("%d,%d,%d,%d,%s,%.3f\n", w, h, bits, op, run.target.name, run.times[3]);
          }
        }
}
