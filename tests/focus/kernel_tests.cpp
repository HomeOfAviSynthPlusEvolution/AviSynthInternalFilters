// SPDX-License-Identifier: GPL-2.0-or-later
#include "focus/kernel.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <future>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
void check(bool condition, const char* message) {
  ++checks;
  if (!condition)
    throw std::runtime_error(message);
}
struct Buffer {
  std::vector<uint8_t> storage;
  uint8_t* data;
  int stride, height;
  Buffer(int row_bytes, int h)
      : storage(((row_bytes + 31) & ~31) * h + 96, 0xA5),
        data(reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(storage.data()) + 31) & ~uintptr_t(31))),
        stride((row_bytes + 31) & ~31), height(h) {}
  bool guards() const {
    return std::all_of(storage.data(), static_cast<const uint8_t*>(data), [](uint8_t v) { return v == 0xA5; }) &&
           std::all_of(static_cast<const uint8_t*>(data + stride * height), storage.data() + storage.size(),
                       [](uint8_t v) { return v == 0xA5; });
  }
};

void blur_scalar_reference() {
  for (int bits : {8, 10, 12, 14, 16, 32})
    for (int width : {1, 2, 7, 17, 33, 67}) {
      const int size = bits == 8 ? 1 : bits == 32 ? 4 : 2;
      const int peak = bits == 32 ? 1 : (1 << bits) - 1;
      Buffer src(width * size, 3), dst(width * size, 3), vertical(width * size, 3), scratch(width * size, 1);
      for (int y = 0; y < 3; ++y)
        for (int x = 0; x < width; ++x) {
          const int value = (x * 157 + y * 47) & peak;
          if (size == 1)
            src.data[y * src.stride + x] = static_cast<uint8_t>(value);
          else if (size == 2)
            reinterpret_cast<uint16_t*>(src.data + y * src.stride)[x] = static_cast<uint16_t>(value);
          else
            reinterpret_cast<float*>(src.data + y * src.stride)[x] = float(x * 7 - y * 3) / 19;
        }
      auto read = [&](const Buffer& b, int x, int y) -> double {
        const uint8_t* p = b.data + y * b.stride;
        return size == 1   ? p[x]
               : size == 2 ? reinterpret_cast<const uint16_t*>(p)[x]
                           : reinterpret_cast<const float*>(p)[x];
      };
      for (int half : {10923, 16384, 32768, 49152, 65536}) {
        const float amount = float(half) / 32768;
        check(aif_focus_horizontal(src.data, src.stride, dst.data, dst.stride, width * size, 3, bits, AIF_FOCUS_PLANAR,
                                   half, amount, 0) == 0,
              "scalar horizontal status");
        std::memcpy(vertical.data, src.data, src.stride * 3);
        check(aif_focus_vertical(vertical.data, vertical.stride, width * size, 3, bits, half, amount, scratch.data,
                                 scratch.stride, 0) == 0,
              "scalar vertical status");
        for (int y = 0; y < 3; ++y)
          for (int x = 0; x < width; ++x)
            for (bool v : {false, true}) {
              const double c = read(src, x, y);
              const double left = v ? read(src, x, std::max(0, y - 1)) : read(src, std::max(0, x - 1), y);
              const double right = v ? read(src, x, std::min(2, y + 1)) : read(src, std::min(width - 1, x + 1), y);
              double expected;
              if (size == 4)
                expected = float(c) * amount + (float(left) + float(right)) * ((1.0f - amount) / 2);
              else
                expected = std::clamp(std::floor((c * half * 2 + (left + right) * (32768 - half) + 32768) / 65536), 0.0,
                                      double(peak));
              check(read(v ? vertical : dst, x, y) == expected, "scalar adjustment reference");
            }
        check(dst.guards() && vertical.guards() && scratch.guards(), "scalar guards");
      }
    }
}

void temporal_and_sad() {
  for (int bits : {8, 10, 16, 32})
    for (uint32_t cpu : {0u, aif_focus_supported_cpu()})
      for (int width : {1, 7, 17, 37})
        for (unsigned threshold : {0u, 12u, 255u}) {
          const int size = bits == 8 ? 1 : bits == 32 ? 4 : 2;
          Buffer center(width * size, 1), first(width * size, 1), second(width * size, 1);
          std::vector<double> original(width), expected(width);
          for (int x = 0; x < width; ++x) {
            int c = 50 + x % 50, a = c + 3, b = c + 40;
            if (size == 1) {
              center.data[x] = uint8_t(c);
              first.data[x] = uint8_t(a);
              second.data[x] = uint8_t(b);
            } else if (size == 2) {
              c <<= bits - 8;
              a <<= bits - 8;
              b <<= bits - 8;
              reinterpret_cast<uint16_t*>(center.data)[x] = uint16_t(c);
              reinterpret_cast<uint16_t*>(first.data)[x] = uint16_t(a);
              reinterpret_cast<uint16_t*>(second.data)[x] = uint16_t(b);
            } else {
              reinterpret_cast<float*>(center.data)[x] = float(c) / 255;
              reinterpret_cast<float*>(first.data)[x] = float(a) / 255;
              reinterpret_cast<float*>(second.data)[x] = float(b) / 255;
            }
            original[x] = c;
            const int sum = c + (threshold >= 3 ? a : c) + (threshold >= 40 ? b : c);
            expected[x] = size == 1   ? (sum * (32768 / 3) + 16384) >> 15
                          : size == 2 ? std::nearbyintf(float(sum) * (1.0f / 3))
                                      : 0;
          }
          int64_t sad = -1;
          check(aif_focus_sad(center.data, first.data, center.stride, first.stride, width * size, 1, bits, cpu, &sad) ==
                    0,
                "sad status");
          if (size != 4)
            check(sad == width * 3, "sad normalization");
          const uint8_t* neighbors[] = {first.data, second.data};
          check(aif_focus_temporal_line(center.data, neighbors, 2, width * size, bits, AIF_FOCUS_PLANAR, threshold,
                                        threshold, cpu) == 0,
                "temporal status");
          for (int x = 0; x < width; ++x) {
            if (size == 1)
              check(center.data[x] == expected[x], "temporal u8 reference");
            else if (size == 2)
              check(reinterpret_cast<uint16_t*>(center.data)[x] == expected[x], "temporal u16 reference");
            else {
              const float c = float(original[x]) / 255;
              const float a = reinterpret_cast<float*>(first.data)[x], b = reinterpret_cast<float*>(second.data)[x];
              const float value = ((c + (threshold == 255 || std::abs(c - b) <= threshold / 255.0f ? b : c)) +
                                   (threshold == 255 || std::abs(c - a) <= threshold / 255.0f ? a : c)) /
                                  3;
              check(reinterpret_cast<float*>(center.data)[x] == value, "temporal float order");
            }
          }
          check(center.guards() && first.guards() && second.guards(), "temporal guards");
        }
}

void spatial_edges() {
  for (int width : {2, 4, 8, 34})
    for (int height : {1, 3})
      for (int radius : {0, 1, 3, 32}) {
        Buffer src(width * 2, height), dst(width * 2, height);
        for (int y = 0; y < height; ++y)
          for (int x = 0; x < width * 2; ++x)
            src.data[y * src.stride + x] = uint8_t(20 + x + y);
        check(aif_focus_spatial_yuy2(src.data, src.stride, dst.data, dst.stride, width * 2, height, radius, 0, 0) == 0,
              "spatial status");
        for (int y = 0; y < height; ++y)
          check(std::memcmp(src.data + y * src.stride, dst.data + y * dst.stride, width * 2) == 0,
                "zero-threshold spatial identity");
        check(src.guards() && dst.guards(), "spatial guards");
      }
}

void invalid_inputs() {
  alignas(32) uint8_t source[64]{}, destination[64], scratch[64];
  std::memset(destination, 0x5A, sizeof(destination));
  check(aif_focus_horizontal(source, 64, destination, 64, 64, 1, 7, AIF_FOCUS_PLANAR, 16384, .5f, 0) != 0,
        "reject bit depth");
  check(aif_focus_horizontal(source, 64, destination, 64, 64, 1, 8, AIF_FOCUS_PLANAR, 16384, NAN, 0) != 0,
        "reject NaN amount");
  check(aif_focus_vertical(destination, 64, 64, 1, 8, 16384, .5f, scratch, 1, 0) != 0, "reject scratch extent");
  check(aif_focus_temporal_line(destination, nullptr, 2, 64, 8, 0, 12, 12, 0) != 0, "reject missing neighbors");
  check(aif_focus_spatial_yuy2(source, 64, destination, 64, 64, 1, 33, 0, 0) != 0, "reject radius");
  check(std::all_of(destination, destination + 64, [](uint8_t v) { return v == 0x5A; }), "invalid calls do not write");
}

void native_adjustment_boundaries() {
  std::vector<uint32_t> targets{0};
  const uint32_t supported = aif_focus_supported_cpu();
  for (uint32_t mask : {1u, 3u, 7u, 15u})
    if ((mask & supported) == mask)
      targets.push_back(mask);
  for (uint32_t cpu : targets)
    for (int bits : {8, 10, 16, 32})
      for (int width : {1, 2, 3, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65})
        for (int height : {1, 3})
          for (int layout : {AIF_FOCUS_PLANAR, AIF_FOCUS_RGB3, AIF_FOCUS_RGB4, AIF_FOCUS_YUY2}) {
            if ((bits == 32 && layout != AIF_FOCUS_PLANAR) || (layout == AIF_FOCUS_YUY2 && (bits != 8 || width % 2)))
              continue;
            const int size = bits == 8 ? 1 : bits == 32 ? 4 : 2;
            const int channels = layout == AIF_FOCUS_PLANAR ? 1
                                 : layout == AIF_FOCUS_RGB3 ? 3
                                 : layout == AIF_FOCUS_RGB4 ? 4
                                                            : 2;
            const int row = width * channels * size;
            Buffer src(row, height), expected(row, height), actual(row, height), vertical(row, height), scratch(row, 1);
            for (int y = 0; y < height; ++y)
              for (int x = 0; x < row / size; ++x) {
                const unsigned value = (x * 73 + y * 131) & 255;
                auto* p = src.data + y * src.stride;
                if (size == 1)
                  p[x] = uint8_t(value);
                else if (size == 2)
                  reinterpret_cast<uint16_t*>(p)[x] = uint16_t(value << (bits - 8));
                else
                  reinterpret_cast<float*>(p)[x] = float(value) / 191 - .25f;
              }
            const auto source_before = src.storage;
            check(aif_focus_horizontal(src.data, src.stride, expected.data, expected.stride, row, height, bits, layout,
                                       16384, .5f, 0) == 0,
                  "horizontal boundary reference status");
            check(aif_focus_horizontal(src.data, src.stride, actual.data, actual.stride, row, height, bits, layout,
                                       16384, .5f, cpu) == 0,
                  "horizontal boundary SIMD status");
            for (int y = 0; y < height; ++y)
              check(std::memcmp(expected.data + y * expected.stride, actual.data + y * actual.stride, row) == 0,
                    "horizontal exact-half boundary");
            std::memcpy(expected.data, src.data, src.stride * height);
            std::memcpy(vertical.data, src.data, src.stride * height);
            check(aif_focus_vertical(expected.data, expected.stride, row, height, bits, 16384, .5f, scratch.data,
                                     scratch.stride, 0) == 0,
                  "vertical boundary reference status");
            check(aif_focus_vertical(vertical.data, vertical.stride, row, height, bits, 16384, .5f, scratch.data,
                                     scratch.stride, cpu) == 0,
                  "vertical boundary SIMD status");
            for (int y = 0; y < height; ++y)
              check(std::memcmp(expected.data + y * expected.stride, vertical.data + y * vertical.stride, row) == 0,
                    "vertical exact-half boundary");
            check(src.storage == source_before, "adjustment source read-only");
            check(src.guards() && expected.guards() && actual.guards() && vertical.guards() && scratch.guards(),
                  "native adjustment guards");
          }
}

void vertical_sharpen_extremes() {
  const auto supported = aif_focus_supported_cpu();
  for (uint32_t cpu : {1u, 3u, 7u, 15u}) {
    if ((cpu & supported) != cpu)
      continue;
    for (int bits : {8, 16})
      for (int width : {15, 16, 17, 31, 32, 33, 63, 64, 65})
        for (int half : {32768, 48901, 65536}) {
          const int size = bits / 8, peak = (1 << bits) - 1, row = width * size;
          Buffer src(row, 3), actual(row, 3), scratch(row, 1);
          for (int y = 0; y < 3; ++y)
            for (int x = 0; x < width; ++x) {
              const int value = x % 3 == 0 ? 0 : x % 3 == 1 ? peak : ((x * 7919 + y * 17389) & peak);
              if (bits == 8)
                src.data[y * src.stride + x] = uint8_t(value);
              else
                reinterpret_cast<uint16_t*>(src.data + y * src.stride)[x] = uint16_t(value);
            }
          auto read = [bits](const Buffer& b, int x, int y) -> int {
            const auto* line = b.data + y * b.stride;
            return bits == 8 ? line[x] : reinterpret_cast<const uint16_t*>(line)[x];
          };
          std::memcpy(actual.data, src.data, src.stride * 3);
          check(aif_focus_vertical(actual.data, actual.stride, row, 3, bits, half, half / 32768.0f, scratch.data,
                                   scratch.stride, cpu) == 0,
                "vertical sharpening status");
          const bool quant = ((cpu & 8) && row >= 32) || ((cpu & (bits == 8 ? 1 : 5)) && row >= 16);
          const int weight = quant ? ((half + 256) >> 9) * 512 : half;
          for (int y = 0; y < 3; ++y)
            for (int x = 0; x < width; ++x) {
              const int64_t c = int64_t(read(src, x, y));
              const int64_t outer =
                  int64_t(read(src, x, std::max(0, y - 1))) + int64_t(read(src, x, std::min(2, y + 1)));
              const auto expected =
                  std::clamp((2 * c * weight + outer * (32768 - weight) + 32768) >> 16, int64_t(0), int64_t(peak));
              check(read(actual, x, y) == expected, "vertical sharpening quantization and block order");
            }
          check(src.guards() && actual.guards() && scratch.guards(), "vertical sharpening guards");
        }
  }
}

void wide_sad() {
  constexpr int width = 4096, height = 2160;
  Buffer a(width, height), b(width, height);
  std::memset(a.data, 0, a.stride * height);
  std::memset(b.data, 255, b.stride * height);
  int64_t result = -1;
  check(aif_focus_sad(a.data, b.data, a.stride, b.stride, width, height, 8, aif_focus_supported_cpu(), &result) == 0,
        "large SAD status");
  check(result == int64_t(width) * height * 255, "SAD exceeds signed 32-bit without wrapping");
  alignas(32) float x[8]{NAN}, y[8]{};
  result = 123;
  check(aif_focus_sad(x, y, 32, 32, 32, 1, 32, 0, &result) != 0 && result == 123,
        "non-finite SAD rejected without writing result");
}

void concurrent_calls() {
  auto run = [](uint32_t cpu) {
    Buffer source(67, 3), output(67, 3);
    for (int y = 0; y < 3; ++y)
      for (int x = 0; x < 67; ++x)
        source.data[y * source.stride + x] = uint8_t(x * 17 + y * 3);
    for (int i = 0; i < 100; ++i)
      if (aif_focus_horizontal(source.data, source.stride, output.data, output.stride, 67, 3, 8, AIF_FOCUS_PLANAR,
                               16384, .5f, cpu) != 0)
        throw std::runtime_error("concurrent kernel status");
    std::vector<uint8_t> result;
    for (int y = 0; y < 3; ++y)
      result.insert(result.end(), output.data + y * output.stride, output.data + y * output.stride + 67);
    return result;
  };
  auto a = std::async(std::launch::async, run, 0u);
  auto b = std::async(std::launch::async, run, aif_focus_supported_cpu());
  check(a.get() == b.get(), "independent concurrent CPU policies");
}
} // namespace

int main() {
  try {
    blur_scalar_reference();
    temporal_and_sad();
    spatial_edges();
    invalid_inputs();
    native_adjustment_boundaries();
    vertical_sharpen_extremes();
    wide_sad();
    concurrent_calls();
    std::printf("%d checks passed; CPU mask %u\n", checks, aif_focus_supported_cpu());
  } catch (const std::exception& e) {
    std::fprintf(stderr, "check %d failed: %s\n", checks, e.what());
    return 1;
  }
}
