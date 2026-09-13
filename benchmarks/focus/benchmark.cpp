// SPDX-License-Identifier: GPL-2.0-or-later
#include "focus/kernel.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
struct Buffer {
  std::vector<uint8_t> storage;
  uint8_t* p;
  Buffer(size_t bytes)
      : storage(bytes + 32),
        p(reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(storage.data()) + 31) & ~uintptr_t(31))) {}
};
void check(int status) {
  if (status)
    throw std::runtime_error("kernel rejected benchmark input");
}
template <class F>
double measure(F run, size_t samples) {
  run();
  run();
  std::vector<double> times;
  for (int trial = 0; trial < 5; ++trial) {
    const auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < 8; ++iteration)
      run();
    const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
    times.push_back(ns / (8 * samples));
  }
  std::sort(times.begin(), times.end());
  return times[2];
}
int main() {
  try {
    constexpr int width = 1920, height = 1080;
    const uint32_t supported = aif_focus_supported_cpu();
    std::puts("backend,operation,bits,width,height,cpu,ns_per_sample");
    for (int bits : {8, 10, 16, 32}) {
      const int size = bits == 8 ? 1 : bits == 32 ? 4 : 2, row = width * size;
      const size_t samples = size_t(width) * height, bytes = size_t(row) * height;
      Buffer source(bytes), other(bytes), dest(bytes), scratch(row);
      uint32_t state = 0x93175;
      for (size_t x = 0; x < samples; ++x) {
        state = state * 1664525u + 1013904223u;
        if (bits == 8) {
          source.p[x] = uint8_t(state >> 24);
          other.p[x] = uint8_t(state >> 16);
        } else if (bits == 32) {
          reinterpret_cast<float*>(source.p)[x] = float(state & 65535) / 65535;
          reinterpret_cast<float*>(other.p)[x] = float(state >> 16) / 65535;
        } else {
          reinterpret_cast<uint16_t*>(source.p)[x] = uint16_t(state & ((1u << bits) - 1));
          reinterpret_cast<uint16_t*>(other.p)[x] = uint16_t((state >> 16) & ((1u << bits) - 1));
        }
      }
      for (bool scalar : {true, false}) {
        if (!scalar && !supported)
          continue;
        const uint32_t cpu = scalar ? 0 : supported;
        const char* name = scalar ? "scalar" : "highway";
        auto report = [&](const char* op, double ns) {
          std::printf("%s,%s,%d,%d,%d,%u,%.4f\n", name, op, bits, width, height, cpu, ns);
        };
        report("horizontal", measure(
                                 [&] {
                                   check(aif_focus_horizontal(source.p, row, dest.p, row, row, height, bits, 0, 48901,
                                                              48901 / 32768.0f, cpu));
                                 },
                                 samples));
        report("vertical_with_restore", measure(
                                            [&] {
                                              std::memcpy(dest.p, source.p, bytes);
                                              check(aif_focus_vertical(dest.p, row, row, height, bits, 48901,
                                                                       48901 / 32768.0f, scratch.p, row, cpu));
                                            },
                                            samples));
        report("temporal4_with_restore",
               measure(
                   [&] {
                     std::memcpy(dest.p, source.p, bytes);
                     for (int y = 0; y < height; ++y) {
                       const uint8_t* inputs[] = {source.p + y * row, other.p + y * row, source.p + y * row,
                                                  other.p + y * row};
                       check(aif_focus_temporal_line(dest.p + y * row, inputs, 4, row, bits, 0, 12, 12, cpu));
                     }
                   },
                   samples));
        report("sad", measure(
                          [&] {
                            int64_t result = -1;
                            check(aif_focus_sad(source.p, other.p, row, row, row, height, bits, cpu, &result));
                            if (result < 0)
                              throw std::runtime_error("invalid SAD");
                          },
                          samples));
      }
    }
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
