// SPDX-License-Identifier: GPL-2.0-or-later
#include "focus/kernel.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define AIF_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define AIF_ASAN 1
#endif
#ifdef AIF_ASAN
#include <sanitizer/asan_interface.h>
#endif
namespace {
void check(bool ok) {
  if (!ok)
    throw std::runtime_error("active-span or status check failed");
}
struct Plane {
  std::vector<uint8_t> storage;
  uint8_t* p;
  int row, stride, height;
  Plane(int r, int h)
      : storage(size_t((r + 31) & ~31) * h + 96, 0xA5),
        p(reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(storage.data()) + 31) & ~uintptr_t(31))), row(r),
        stride((r + 31) & ~31), height(h) {
    for (int y = 0; y < h; ++y)
      std::memset(p + y * stride, 0, row);
  }
  void poison() {
#ifdef AIF_ASAN
    __asan_poison_memory_region(storage.data(), p - storage.data());
    for (int y = 0; y < height; ++y)
      __asan_poison_memory_region(p + y * stride + row, stride - row);
    __asan_poison_memory_region(p + height * stride, storage.data() + storage.size() - (p + height * stride));
#endif
  }
  void unpoison() {
#ifdef AIF_ASAN
    __asan_unpoison_memory_region(storage.data(), storage.size());
#endif
  }
  void guards() {
    unpoison();
    for (auto* q = storage.data(); q < p; ++q)
      check(*q == 0xA5);
    for (int y = 0; y < height; ++y)
      for (int x = row; x < stride; ++x)
        check(p[y * stride + x] == 0xA5);
    for (auto* q = p + height * stride; q < storage.data() + storage.size(); ++q)
      check(*q == 0xA5);
    poison();
  }
  ~Plane() { unpoison(); }
};
} // namespace
int main() {
  try {
    int cases = 0;
    const auto supported = aif_focus_supported_cpu();
    for (int bits : {8, 10, 16, 32})
      for (int layout : {0, 1, 2, 3}) {
        if ((bits == 32 && layout) || (layout == 3 && bits != 8))
          continue;
        const int size = bits == 8 ? 1 : bits == 32 ? 4 : 2;
        const int channels = layout == 1 ? 3 : layout == 2 ? 4 : layout == 3 ? 2 : 1;
        for (int width : {1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127}) {
          if (layout == 3 && width % 2)
            continue;
          const int row = width * channels * size;
          for (uint32_t request : {0u, 1u, 2u, 4u, 8u, 15u, 256u, 768u}) {
            const uint32_t cpu = request & supported;
            Plane source(row, 2), other(row, 2), dst(row, 2), scratch(row, 1);
            source.poison();
            other.poison();
            dst.poison();
            check(!aif_focus_horizontal(source.p, source.stride, dst.p, dst.stride, row, 2, bits, layout, 48901,
                                        1.49234f, cpu));
            check(
                !aif_focus_vertical(dst.p, dst.stride, row, 2, bits, 48901, 1.49234f, scratch.p, scratch.stride, cpu));
            int64_t sad = -1;
            check(!aif_focus_sad(source.p, other.p, source.stride, other.stride, row, 2, bits, cpu, &sad));
            check(sad == 0);
            if (layout == 0 || layout == 3) {
              const uint8_t* inputs[] = {source.p, other.p};
              check(!aif_focus_temporal_line(dst.p, inputs, 2, row, bits, layout, 255, 12, cpu));
            }
            source.guards();
            other.guards();
            dst.guards();
            ++cases;
          }
        }
      }
    std::printf("%d active-span memory cases passed; ASan poisoning %s\n", cases,
#ifdef AIF_ASAN
                "enabled"
#else
                "disabled (guard checks only)"
#endif
    );
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
