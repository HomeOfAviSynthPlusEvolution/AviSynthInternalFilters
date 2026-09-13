// SPDX-License-Identifier: GPL-2.0-or-later
#include "greyscale/kernel.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
int main() {
#ifdef _WIN32
  SetProcessAffinityMask(GetCurrentProcess(), 4);
  SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif
  struct Target {
    const char* name;
    uint32_t mask;
  };
  const Target targets[] = {{"C", 0},          {"SSE2", 1},      {"SSSE3", 2},    {"SSE4", 8},
                            {"AVX2", 16},      {"AVX3", 32},     {"AVX3_DL", 64}, {"AVX3_ZEN4", 128},
                            {"AVX3_SPR", 256}, {"AVX10_2", 512}, {"auto", ~0u}};
  std::puts("width,height,bits,layout,target,median_us");
  for (int w : {640, 1920})
    for (int bits : {8, 16, 32})
      for (int layout : {0, 3, 4, 5, 6}) {
        if (((layout == 3 || layout == 4) && bits == 32) || (layout == 5 && bits != 8))
          continue;
        const int h = w == 640 ? 360 : 1080, bytes = bits == 8 ? 1 : bits == 32 ? 4 : 2;
        const int components = layout == 3 ? 3 : layout == 4 ? 4 : layout == 5 ? 2 : 1;
        const int pitch = (w * bytes * components + 63) & ~63;
        std::vector<uint32_t> input(size_t(pitch) * h * 3 / 4, 0), data = input, expected = input;
        auto* raw = reinterpret_cast<uint8_t*>(input.data());
        for (size_t i = 0; i < input.size() * 4 / bytes; ++i) {
          if (bits == 8)
            raw[i] = uint8_t(i * 37);
          else if (bits == 16)
            reinterpret_cast<uint16_t*>(raw)[i] = uint16_t(i * 37);
          else
            reinterpret_cast<float*>(raw)[i] = float(i % 251) / 250;
        }
        struct Run {
          Target target;
          aif_greyscale_plan* plan = nullptr;
          std::array<double, 7> times{};
        };
        std::vector<Run> runs;
        for (auto t : targets) {
          if (t.mask && t.mask != ~0u && !(t.mask & aif_greyscale_supported_cpu()))
            continue;
          Run r{t};
          if (layout < 5 && aif_greyscale_create(.299, .114, bits, 1, 1, t.mask, &r.plan))
            return 1;
          runs.push_back(r);
        }
        auto apply = [&](Run& r) {
          auto* d = reinterpret_cast<uint8_t*>(data.data());
          uint8_t* planes[] = {d, d + size_t(pitch) * h, d + size_t(pitch) * h * 2};
          int pitches[] = {pitch, pitch, pitch};
          if (layout == 5)
            return aif_greyscale_yuy2(d, pitch, w, h, r.target.mask);
          if (layout == 6)
            return aif_greyscale_chroma(d, pitch, w, h, bits, r.target.mask);
          return aif_greyscale_rgb(r.plan, planes, pitches, w, h, layout != 0, components == 4 ? 4 : 3);
        };
        for (auto& r : runs) {
          data = input;
          if (apply(r))
            return 2;
          if (r.target.mask == 0)
            expected = data;
          else if (data != expected)
            return 3;
        }
        for (size_t round = 0; round < 7; ++round)
          for (size_t j = 0; j < runs.size(); ++j) {
            auto& r = runs[(j + round) % runs.size()];
            data = input;
            for (int warm = 0; warm < 2; ++warm)
              if (apply(r))
                return 4;
            auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < 4; ++i)
              if (apply(r))
                return 4;
            r.times[round] =
                std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 4;
          }
        for (auto& r : runs) {
          std::sort(r.times.begin(), r.times.end());
          std::printf("%d,%d,%d,%d,%s,%.3f\n", w, h, bits, layout, r.target.name, r.times[3]);
          aif_greyscale_destroy(r.plan);
        }
      }
}
