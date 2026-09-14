// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstring>
HWY_BEFORE_NAMESPACE();
namespace aif::rotation {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
#if HWY_TARGET != HWY_SCALAR && HWY_TARGET != HWY_EMU128
template <class D>
HWY_INLINE hn::VFromD<D> LoadRows(D d, const uint8_t* s, int pitch) {
  if constexpr (hn::MaxLanes(d) * sizeof(hn::TFromD<D>) <= 16) {
    return hn::LoadU(d, reinterpret_cast<const hn::TFromD<D>*>(s));
  } else {
    const hn::Half<D> half;
    return hn::Combine(d, LoadRows(half, s + ptrdiff_t(hn::Lanes(half)) * pitch, pitch), LoadRows(half, s, pitch));
  }
}
template <class T, size_t Step, class D, size_t N>
HWY_INLINE void TransposeStage(D d, hn::VFromD<D> (&rows)[N]) {
  using Lane = hwy::UnsignedFromSize<sizeof(T) * Step>;
  const hn::Repartition<Lane, D> wide;
  hn::VFromD<D> next[N];
  for (size_t base = 0; base < N; base += 2 * Step)
    for (size_t i = 0; i < Step; ++i) {
      const auto a = hn::BitCast(wide, rows[base + i]);
      const auto b = hn::BitCast(wide, rows[base + Step + i]);
      next[base + 2 * i] = hn::BitCast(d, hn::InterleaveLower(wide, a, b));
      next[base + 2 * i + 1] = hn::BitCast(d, hn::InterleaveUpper(wide, a, b));
    }
  for (size_t i = 0; i < N; ++i)
    rows[i] = next[i];
  if constexpr (2 * Step < N)
    TransposeStage<T, 2 * Step>(d, rows);
}
template <class D>
HWY_INLINE void TurnTiles(D tile, const uint8_t* s, uint8_t* d, int w, int h, int sp, int dp, int& y) {
  using T = hn::TFromD<D>;
  constexpr int n = hn::MaxLanes(tile) < 16 / sizeof(T) ? hn::MaxLanes(tile) : 16 / sizeof(T);
  const int tile_height = int(hn::Lanes(tile));
  for (; y + tile_height <= h; y += tile_height) {
    int x = 0;
    for (; x + n <= w; x += n) {
      hn::VFromD<D> columns[n];
      for (int i = 0; i < n; ++i)
        columns[i] = LoadRows(tile, s + ptrdiff_t(y + i) * sp + x * sizeof(T), sp);
      if constexpr (n > 1)
        TransposeStage<T, 1>(tile, columns);
      for (int i = 0; i < n; ++i)
        hn::StoreU(columns[i], tile, reinterpret_cast<T*>(d + ptrdiff_t(x + i) * dp) + y);
    }
    for (; x < w; ++x)
      for (int i = 0; i < tile_height; ++i)
        std::memcpy(d + ptrdiff_t(x) * dp + (y + i) * sizeof(T), s + ptrdiff_t(y + i) * sp + x * sizeof(T), sizeof(T));
  }
  if constexpr (hn::MaxLanes(tile) > 1)
    TurnTiles(hn::Half<D>(), s, d, w, h, sp, dp, y);
}
#if HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3 && !defined(HWY_DISABLE_CACHE_CONTROL)
template <class D>
HWY_INLINE void TurnStream32(D tile, const uint8_t* s, uint8_t* d, int w, int h, int sp, int dp, int& y) {
  static_assert(hn::MaxLanes(tile) == 8);
  constexpr int n = 8;
  for (; y + 2 * n <= h; y += 2 * n) {
    int x = 0;
    for (; x + n <= w; x += n) {
      hn::VFromD<D> output[16];
      for (int group = 0; group < 16; group += 8) {
        hn::VFromD<D> rows[8], t[8], q[8], columns[8];
        const hn::Repartition<uint64_t, D> wide;
        for (int i = 0; i < 8; ++i)
          rows[i] = hn::LoadU(tile, reinterpret_cast<const uint32_t*>(s + ptrdiff_t(y + group + i) * sp) + x);
        for (int i = 0; i < 8; i += 2) {
          t[i] = hn::InterleaveLower(tile, rows[i], rows[i + 1]);
          t[i + 1] = hn::InterleaveUpper(tile, rows[i], rows[i + 1]);
        }
        for (int base = 0; base < 8; base += 4) {
          for (int i = 0; i < 2; ++i) {
            q[base + 2 * i] = hn::BitCast(
                tile, hn::InterleaveLower(wide, hn::BitCast(wide, t[base + i]), hn::BitCast(wide, t[base + i + 2])));
            q[base + 2 * i + 1] = hn::BitCast(
                tile, hn::InterleaveUpper(wide, hn::BitCast(wide, t[base + i]), hn::BitCast(wide, t[base + i + 2])));
          }
        }
        for (int i = 0; i < 4; ++i) {
          columns[i] = hn::ConcatLowerLower(tile, q[i + 4], q[i]);
          columns[i + 4] = hn::ConcatUpperUpper(tile, q[i + 4], q[i]);
        }
        for (int i = 0; i < 8; ++i)
          output[group + i] = columns[i];
      }
      const hn::CappedTag<uint32_t, 16> full;
      for (int i = 0; i < 8; ++i)
        hn::Stream(hn::Combine(full, output[i + 8], output[i]), full,
                   reinterpret_cast<uint32_t*>(d + ptrdiff_t(x + i) * dp) + y);
    }
    for (; x < w; ++x)
      for (int i = 0; i < 2 * n; ++i)
        std::memcpy(d + ptrdiff_t(x) * dp + (y + i) * 4, s + ptrdiff_t(y + i) * sp + x * 4, 4);
  }
}

#endif

template <class T>
void Typed(const uint8_t* s, uint8_t* d, int row, int h, int sp, int dp, int op) {
  const int w = row / sizeof(T);
  const hn::CappedTag<T, 16 / sizeof(T)> tag;
  const int lanes = int(hn::Lanes(tag));
  if (op >= 2) {
    for (int y = 0; y < h; ++y) {
      const T* src = reinterpret_cast<const T*>(s + ptrdiff_t(y) * sp);
      T* dst = reinterpret_cast<T*>(d + ptrdiff_t(op == 3 ? y : h - 1 - y) * dp);
      int x = 0;
      if (op == 4) {
        std::memcpy(dst, src, row);
        continue;
      }
      for (; x + 4 * lanes <= w; x += 4 * lanes) {
        const auto a = hn::Reverse(tag, hn::LoadU(tag, src + x));
        const auto b = hn::Reverse(tag, hn::LoadU(tag, src + x + lanes));
        const auto c = hn::Reverse(tag, hn::LoadU(tag, src + x + 2 * lanes));
        const auto e = hn::Reverse(tag, hn::LoadU(tag, src + x + 3 * lanes));
        hn::StoreU(e, tag, dst + w - x - 4 * lanes);
        hn::StoreU(c, tag, dst + w - x - 3 * lanes);
        hn::StoreU(b, tag, dst + w - x - 2 * lanes);
        hn::StoreU(a, tag, dst + w - x - lanes);
      }
      for (; x + lanes <= w; x += lanes) {
        const auto v = hn::LoadU(tag, src + x);
        hn::StoreU(hn::Reverse(tag, v), tag, dst + w - x - lanes);
      }
      for (; x < w; ++x)
        dst[w - 1 - x] = src[x];
    }
    return;
  }
  if (op == 1) {
    s += ptrdiff_t(h - 1) * sp;
    sp = -sp;
  } else {
    d += ptrdiff_t(w - 1) * dp;
    dp = -dp;
  }
  int y = 0;
  // Zen 4 measurements favor 256-bit transpose blocks for wider pixels and
  // AVX3's byte shuffles. AVX3_DL and later can use efficient 512-bit byte moves.
#if HWY_TARGET == HWY_AVX3
  const hn::CappedTag<T, 32 / sizeof(T)> turn_tag;
#else
  const hn::CappedTag<T, sizeof(T) == 1 ? 64 : 32 / sizeof(T)> turn_tag;
#endif
#if HWY_ARCH_X86 && HWY_TARGET <= HWY_AVX3 && !defined(HWY_DISABLE_CACHE_CONTROL)
  if constexpr (sizeof(T) == 4) {
    // Complete cache-line writes avoid allocating the large transposed output in cache.
    if ((reinterpret_cast<uintptr_t>(d) & 63) == 0 && dp % 64 == 0 && size_t(w) * h >= 1920u * 1080u) {
      TurnStream32(turn_tag, s, d, w, h, sp, dp, y);
      hwy::FlushStream();
    }
  }
#endif
  TurnTiles(turn_tag, s, d, w, h, sp, dp, y);
}

void RotatePlane(const uint8_t* s, uint8_t* d, int row, int h, int sp, int dp, int bytes, int op) {
  switch (bytes) {
    case 1:
      return Typed<uint8_t>(s, d, row, h, sp, dp, op);
    case 2:
      return Typed<uint16_t>(s, d, row, h, sp, dp, op);
    case 4:
      return Typed<uint32_t>(s, d, row, h, sp, dp, op);
    case 8:
      return Typed<uint64_t>(s, d, row, h, sp, dp, op);
    default:
      return scalar(s, d, row, h, sp, dp, bytes, op); // RGB24/48 and YUY2 had scalar turns upstream.
  }
}
#endif
} // namespace HWY_NAMESPACE
} // namespace aif::rotation
HWY_AFTER_NAMESPACE();
