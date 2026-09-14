// SPDX-License-Identifier: GPL-2.0-or-later
#include "greyscale/kernel.h"
#include <cstring>
#include <cmath>
#include <limits>
#include <cstdio>
#include <stdexcept>
#include <vector>
void check(bool ok) {
  if (!ok)
    throw std::runtime_error("greyscale kernel failure");
}
void unaligned_bgra(uint32_t cpu) {
  for (int width : {15, 16, 17, 33})
    for (int sf : {0, 1})
      for (int df : {0, 1}) {
        const int pitch = width * 4 + 9, height = 3;
        std::vector<uint8_t> source(pitch * height + 2, 0xAD);
        for (int y = 0; y < height; ++y)
          for (int x = 0; x < width * 4; ++x)
            source[1 + y * pitch + x] = uint8_t(x * 37 + y * 19);
        auto expected = source, actual = source;
        for (int mode = 0; mode < 2; ++mode) {
          uint8_t* data[] = {(mode ? actual : expected).data() + 1};
          int pitches[] = {pitch};
          aif_greyscale_plan* plan = nullptr;
          check(!aif_greyscale_create(.299, .114, 8, sf, df, mode ? cpu : 0, &plan));
          const int result = aif_greyscale_rgb(plan, data, pitches, width, height, 1, 4);
          aif_greyscale_destroy(plan);
          check(!result);
        }
        check(expected == actual);
        check(actual.front() == source.front() && actual.back() == source.back());
        for (int y = 0; y < height; ++y) {
          for (int x = 0; x < width; ++x)
            check(actual[1 + y * pitch + 4 * x + 3] == source[1 + y * pitch + 4 * x + 3]);
          for (int x = width * 4; x < pitch; ++x)
            check(actual[1 + y * pitch + x] == source[1 + y * pitch + x]);
        }
      }
}
void float_specials(uint32_t cpu) {
  constexpr int width = 65, stride = 72, height = 2, plane = stride * height;
  const float values[] = {0.f,
                          -0.f,
                          -4.f,
                          100.f,
                          .12345f,
                          std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::quiet_NaN()};
  for (double kr : {0., .299, .2126}) {
    std::vector<float> a(plane * 3, 19.f), b;
    for (int c = 0; c < 3; ++c)
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
          a[c * plane + y * stride + x] = values[(x + 3 * c + y) % 8];
    b = a;
    for (int mode = 0; mode < 2; ++mode) {
      auto& storage = mode ? b : a;
      uint8_t* data[] = {reinterpret_cast<uint8_t*>(storage.data()), reinterpret_cast<uint8_t*>(storage.data() + plane),
                         reinterpret_cast<uint8_t*>(storage.data() + 2 * plane)};
      int pitches[] = {stride * 4, stride * 4, stride * 4};
      aif_greyscale_plan* plan = nullptr;
      check(!aif_greyscale_create(kr, .0722, 32, 1, 1, mode ? cpu : 0, &plan));
      const int result = aif_greyscale_rgb(plan, data, pitches, width, height, 0, 3);
      aif_greyscale_destroy(plan);
      check(!result);
    }
    for (size_t i = 0; i < a.size(); ++i)
      check(a[i] == b[i] || (std::isnan(a[i]) && std::isnan(b[i])));
  }
}

int main() {
  try {
    for (uint32_t cpu : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
      if (cpu != ~0u && !(cpu & aif_greyscale_supported_cpu()))
        continue;
      float_specials(cpu);
      unaligned_bgra(cpu);
      for (int width : {2, 30, 32, 34, 62, 64, 66}) {
        const int pitch = width * 2 + 17;
        std::vector<uint8_t> expected(pitch * 3, 0x71), actual = expected;
        check(!aif_greyscale_yuy2(expected.data(), pitch, width, 3, 0));
        check(!aif_greyscale_yuy2(actual.data(), pitch, width, 3, cpu));
        check(expected == actual);
        for (int y = 0; y < 3; ++y)
          for (int x = width * 2; x < pitch; ++x)
            check(actual[y * pitch + x] == 0x71);
      }
      for (int bits : {8, 10, 12, 14, 16, 32})
        for (int w : {1, 2, 7, 15, 16, 17, 31, 32, 33, 65}) {
          int bytes = bits == 8 ? 1 : bits == 32 ? 4 : 2, pitch = (w + 8) * bytes, h = 3;
          std::vector<uint32_t> s((pitch * h + 3) / 4, 0x71717171), v = s;
          check(!aif_greyscale_chroma(reinterpret_cast<uint8_t*>(s.data()), pitch, w, h, bits, 0));
          check(!aif_greyscale_chroma(reinterpret_cast<uint8_t*>(v.data()), pitch, w, h, bits, cpu));
          check(s == v);
          for (int y = 0; y < h; ++y)
            for (int x = w * bytes; x < pitch; ++x)
              check(reinterpret_cast<uint8_t*>(v.data())[y * pitch + x] == 0x71);
          for (int packed : {0, 1})
            for (int components : {3, 4}) {
              if (packed && bits == 32)
                continue;
              int row = w * bytes * (packed ? components : 1), p = row + 8 * bytes;
              std::vector<uint32_t> input((p * h * 3 + 3) / 4, 0x71717171);
              auto* raw = reinterpret_cast<uint8_t*>(input.data());
              for (int c = 0; c < (packed ? 1 : 3); ++c)
                for (int y = 0; y < h; ++y)
                  for (int x = 0; x < row / bytes; ++x) {
                    auto* d = raw + c * p * h + y * p + x * bytes;
                    unsigned value = (x * 37 + y * 23 + c * 71);
                    if (bytes == 1)
                      *d = uint8_t(value);
                    else if (bytes == 2) {
                      uint16_t t = value & ((1u << bits) - 1);
                      std::memcpy(d, &t, 2);
                    } else {
                      float t = float(value % 251) / 200 - .1f;
                      std::memcpy(d, &t, 4);
                    }
                  }
              for (int sf : {0, 1})
                for (int df : {0, 1}) {
                  auto a = input, b = input;
                  for (int mode = 0; mode < 2; ++mode) {
                    auto& dst = mode ? b : a;
                    auto* d = reinterpret_cast<uint8_t*>(dst.data());
                    uint8_t* data[] = {d, d + p * h, d + 2 * p * h};
                    int strides[] = {p, p, p};
                    aif_greyscale_plan* plan = nullptr;
                    check(!aif_greyscale_create(.2126, .0722, bits, sf, df, mode ? cpu : 0, &plan));
                    int result = aif_greyscale_rgb(plan, data, strides, w, h, packed, components);
                    aif_greyscale_destroy(plan);
                    check(!result);
                  }
                  check(a == b);
                  const auto* d = reinterpret_cast<const uint8_t*>(b.data());
                  for (int c = 0; c < (packed ? 1 : 3); ++c)
                    for (int y = 0; y < h; ++y) {
                      check(!std::memcmp(d + c * p * h + y * p + row, raw + c * p * h + y * p + row, p - row));
                      if (packed && components == 4)
                        for (int x = 0; x < w; ++x)
                          check(
                              !std::memcmp(d + y * p + (x * 4 + 3) * bytes, raw + y * p + (x * 4 + 3) * bytes, bytes));
                    }
                }
            }
        }
    }
    std::puts("greyscale scalar/Highway, alpha and padding passed");
  } catch (const std::exception& e) {
    std::puts(e.what());
    return 1;
  }
}
