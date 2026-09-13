// SPDX-License-Identifier: GPL-2.0-or-later
#include "limiter/kernel.h"
#include <vector>
#include <cstring>
#include <cstdio>
int main() {
  for (uint32_t cpu : {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u}) {
    if (cpu != ~0u && !(cpu & aif_limiter_supported_cpu()))
      continue;
    {
      uint8_t a[32]{}, b[32]{};
      aif_limiter_limits l{16, 235, 64, 128, 8};
      if (aif_limiter_apply(a, 32, 16, 1, &l, 1, 1, 0) || aif_limiter_apply(b, 32, 16, 1, &l, 1, 1, cpu) ||
          std::memcmp(a, b, 32))
        return 6;
      for (int i = 0; i < 32; ++i)
        if (a[i] != (i % 2 ? 64 : 16))
          return 6;
    }
    {
      std::vector<uint32_t> expected(68, 0x71717171);
      const uint32_t values[] = {0,           0x80000000u, 0x7fc12345u, 0xffc12345u,
                                 0x7f800000u, 0xff800000u, 0x3f800000u, 0x40000000u};
      for (int x = 0; x < 65; ++x)
        expected[x] = values[x % 8];
      auto actual = expected;
      aif_limiter_limits limits{0, 1, 0, 1, 32};
      if (aif_limiter_apply(reinterpret_cast<uint8_t*>(expected.data()), 272, 65, 1, &limits, 0, 0, 0) ||
          aif_limiter_apply(reinterpret_cast<uint8_t*>(actual.data()), 272, 65, 1, &limits, 0, 0, cpu) ||
          expected != actual)
        return 7;
    }
    for (int bits : {8, 10, 12, 14, 16, 32})
      for (int w : {1, 2, 6, 14, 16, 17, 18, 30, 32, 34, 63, 64, 65, 66})
        for (int yuy2 : {0, 1}) {
          if (yuy2 && (bits != 8 || w % 2))
            continue;
          int bytes = bits == 8    ? 1
                      : bits == 32 ? 4
                                   : 2,
              row = w * bytes * (yuy2 ? 2 : 1), pitch = (row + 31) / 4 * 4, h = 3;
          std::vector<uint32_t> a(pitch * h / 4, 0x71717171), b;
          auto* d = reinterpret_cast<uint8_t*>(a.data());
          for (int y = 0; y < h; ++y)
            for (int x = 0; x < row / bytes; ++x) {
              unsigned v = x * 37 + y * 71;
              if (bytes == 1)
                d[y * pitch + x] = uint8_t(v);
              else if (bytes == 2) {
                uint16_t v16 = v & ((1u << bits) - 1);
                std::memcpy(d + y * pitch + x * 2, &v16, 2);
              } else {
                float f = float(v % 251) / 150 - .4f;
                std::memcpy(d + y * pitch + x * 4, &f, 4);
              }
            }
          b = a;
          float scale = bits == 32 ? 1.0f / 255 : float(1 << (bits - 8));
          aif_limiter_limits l{16 * scale, 235 * scale, bits == 32 ? -112 * scale : 16 * scale,
                               bits == 32 ? 112 * scale : 240 * scale, bits};
          if (aif_limiter_apply(d, pitch, w, h, &l, 0, yuy2, 0) ||
              aif_limiter_apply(reinterpret_cast<uint8_t*>(b.data()), pitch, w, h, &l, 0, yuy2, cpu) || a != b)
            return 1;
          for (int y = 0; y < h; ++y)
            for (int x = row; x < pitch; ++x)
              if (d[y * pitch + x] != 0x71)
                return 2;
        }
  }
  // A mixed low/high final 2x2 block must color all four pixels, not x+2 padding.
  for (int bits : {8, 16, 32}) {
    int bytes = bits == 8 ? 1 : bits == 16 ? 2 : 4, pitch = 4 * bytes;
    std::vector<uint32_t> y(pitch * 2 / 4, 0x71717171), u(1, 0), v(1, 0);
    auto* yp = reinterpret_cast<uint8_t*>(y.data());
    for (int row = 0; row < 2; ++row)
      for (int x = 0; x < 2; ++x) {
        if (bits == 8)
          yp[row * pitch + x] = x ? 255 : 0;
        else if (bits == 16) {
          uint16_t z = x ? 65535 : 0;
          std::memcpy(yp + row * pitch + x * 2, &z, 2);
        } else {
          float z = x ? 1 : 0;
          std::memcpy(yp + row * pitch + x * 4, &z, 4);
        }
      }
    uint8_t* data[] = {yp, reinterpret_cast<uint8_t*>(u.data()), reinterpret_cast<uint8_t*>(v.data())};
    int pitches[] = {pitch, 4, 4};
    float scale = bits == 32 ? 1.0f / 255 : float(1 << (bits - 8));
    aif_limiter_limits l{16 * scale, 235 * scale, bits == 32 ? -112 * scale : 16 * scale,
                         bits == 32 ? 112 * scale : 240 * scale, bits};
    if (aif_limiter_show(data, pitches, 2, 2, &l, 1, 1))
      return 3;
    for (int row = 0; row < 2; ++row) {
      for (int x = 0; x < 2; ++x) {
        if (bits == 8) {
          if (yp[row * pitch + x] != 210)
            return 4;
        } else if (bits == 16) {
          uint16_t z;
          std::memcpy(&z, yp + row * pitch + x * 2, 2);
          if (z != 210 * 256)
            return 4;
        } else {
          float z;
          std::memcpy(&z, yp + row * pitch + x * 4, 4);
          if (z != 210 / 255.0f)
            return 4;
        }
      }
      for (int x = 2 * bytes; x < pitch; ++x)
        if (yp[row * pitch + x] != 0x71)
          return 5;
    }
  }
  std::puts("limiter scalar/Highway and diagnostic boundaries passed");
}
