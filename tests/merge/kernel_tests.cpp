#include "merge/kernel.h"
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdio>
bool plan_policy() {
  if (!aif_merge_create(0, nullptr) || !aif_merge_mix_with_plan(nullptr, nullptr, nullptr, 0, 0, 0, 0, 8, 1, .5))
    return false;
  aif_merge_destroy(nullptr);
  aif_merge_plan* plan = nullptr;
  if (aif_merge_create(~0u, &plan))
    return false;
  bool ok = true;
  // Reuse one immutable plan across the size/weight-dependent U16 fallback.
  for (int width : {33, 1920, 33})
    for (double weight : {.5, .625}) {
      const int height = width == 1920 ? 1080 : 3, pitch = width * 2;
      std::vector<uint16_t> base(size_t(width) * height), source(base.size()), legacy;
      for (size_t i = 0; i < base.size(); ++i) {
        base[i] = uint16_t(i * 37);
        source[i] = uint16_t(i * 19 + 11);
      }
      legacy = base;
      ok &= !aif_merge_mix(reinterpret_cast<uint8_t*>(legacy.data()), reinterpret_cast<uint8_t*>(source.data()), pitch,
                           pitch, width, height, 16, 2, weight, ~0u);
      ok &= !aif_merge_mix_with_plan(plan, reinterpret_cast<uint8_t*>(base.data()),
                                     reinterpret_cast<uint8_t*>(source.data()), pitch, pitch, width, height, 16, 2,
                                     weight);
      ok &= base == legacy;
    }
  uint8_t base = 17, source = 29;
  ok &= aif_merge_mix_with_plan(plan, &base, &source, 1, 1, 1, 1, 7, 1, .5) != 0 && base == 17;
  ok &= aif_merge_mix_with_plan(plan, &base, &source, 1, 1, 1, 1, 8, 1, -1) != 0 && base == 17;
  ok &= !aif_merge_mix_with_plan(plan, nullptr, nullptr, 0, 0, 0, 0, 7, 9, -1);
  aif_merge_destroy(plan);
  return ok;
}
int main(int argc, char** argv) {
  if (!plan_policy())
    return 10;
  if (argc > 1 && std::strcmp(argv[1], "plan") == 0)
    return 0;
  const bool half_only = argc > 1 && std::strcmp(argv[1], "u8-half") == 0;
  for (int bits : {8, 10, 12, 14, 16, 32})
    for (int w : {1, 7, 15, 16, 17, 31, 32, 33, 63, 64, 65, 255, 256, 257, 511, 512, 513})
      for (int step : {1, 2})
        for (uint32_t cpu : {0u, 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u, ~0u})
          for (double weight : {0., .0039, .25, .499, .5, .7, 1.}) {
            if (half_only && (bits != 8 || weight != .5))
              continue;
            if (cpu && cpu != ~0u && !(cpu & aif_merge_supported_cpu()))
              continue;
            int bytes = bits == 8 ? 1 : bits == 32 ? 4 : 2;
            if (step == 2 && bits != 8)
              continue;
            int stride = bytes * step, pitch = w * stride + 16, h = 3;
            std::vector<uint32_t> store((size_t(pitch) * h + 3) / 4, 0x71717171u), other = store, simd = store, ref;
            auto b = reinterpret_cast<uint8_t*>(store.data()), s = reinterpret_cast<uint8_t*>(other.data());
            for (int y = 0; y < h; ++y)
              for (int x = 0; x < w; ++x) {
                int off = y * pitch + x * stride;
                unsigned v = (x * 37 + y * 13) & 255;
                if (bytes == 1) {
                  b[off] = uint8_t(v);
                  s[off] = uint8_t(255 - v);
                } else if (bytes == 2) {
                  uint16_t a = uint16_t(v * ((1u << bits) - 1) / 255), c = uint16_t(((1u << bits) - 1) - a);
                  std::memcpy(b + off, &a, 2);
                  std::memcpy(s + off, &c, 2);
                } else {
                  float a = float(v) / 127 - .5f, c = 1 - a;
                  std::memcpy(b + off, &a, 4);
                  std::memcpy(s + off, &c, 4);
                }
              }
            ref = store;
            simd = store;
            auto r = reinterpret_cast<uint8_t*>(ref.data()), d = reinterpret_cast<uint8_t*>(simd.data());
            aif_merge_plan* plan = nullptr;
            if (aif_merge_create(cpu, &plan))
              return 7;
            auto legacy = store;
            if (aif_merge_mix(reinterpret_cast<uint8_t*>(legacy.data()), s, pitch, pitch, w, h, bits, stride, weight,
                              cpu))
              return 8;
            const int planned = aif_merge_mix_with_plan(plan, d, s, pitch, pitch, w, h, bits, stride, weight);
            aif_merge_destroy(plan);
            if (planned || legacy != simd)
              return 9;
            if (aif_merge_mix(r, s, pitch, pitch, w, h, bits, stride, weight, 0))
              return 1;
            for (int y = 0; y < h; ++y) {
              for (int x = 0; x < w; ++x) {
                int off = y * pitch + x * stride;
                if (bytes == 1) {
                  if (std::abs(int(d[off]) - int(r[off])) > 1)
                    return 2;
                  if (step == 2 && d[off + 1] != b[off + 1])
                    return 3;
                } else if (bytes == 2) {
                  if (std::abs(int(*reinterpret_cast<uint16_t*>(d + off)) -
                               int(*reinterpret_cast<uint16_t*>(r + off))) > 1)
                    return 4;
                } else if (!(std::abs(*reinterpret_cast<float*>(d + off) - *reinterpret_cast<float*>(r + off)) <= 1e-6))
                  return 5;
              }
              for (int x = w * stride; x < pitch; ++x)
                if (d[y * pitch + x] != 0x71)
                  return 6;
            }
          }
  std::puts("merge arithmetic and padding passed");
}
