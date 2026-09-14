// SPDX-License-Identifier: GPL-2.0-or-later
// Per-target kernel definitions, intentionally included once per Highway target.
#include "compatibility.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>

HWY_BEFORE_NAMESPACE();
namespace aif::focus {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
#if HWY_TARGET != HWY_SCALAR && HWY_TARGET != HWY_EMU128
// Full vectors touch only active samples. Short vectors use bounded staging;
// unlike the historical intrinsics, no Highway load accesses row padding.
template <class D>
HWY_INLINE hn::VFromD<D> Load(D d, const hn::TFromD<D>* p, size_t count) {
  if (count == hn::Lanes(d))
    return hn::LoadU(d, p);
  HWY_ALIGN hn::TFromD<D> tmp[hn::MaxLanes(d)] = {};
  std::memcpy(tmp, p, count * sizeof(*p));
  return hn::LoadU(d, tmp);
}
template <class D>
HWY_INLINE void Store(hn::VFromD<D> v, D d, hn::TFromD<D>* p, size_t count) {
  if (count == hn::Lanes(d)) {
    hn::StoreU(v, d, p);
    return;
  }
  HWY_ALIGN hn::TFromD<D> tmp[hn::MaxLanes(d)];
  hn::StoreU(v, d, tmp);
  std::memcpy(p, tmp, count * sizeof(*p));
}

template <bool Quantized, class D>
HWY_INLINE hn::VFromD<D> Adjust(D d, hn::VFromD<D> c, hn::VFromD<D> l, hn::VFromD<D> r, int half, float amount,
                                int peak) {
  if constexpr (std::is_same_v<hn::TFromD<D>, float>) {
    return hn::Add(hn::Mul(c, hn::Set(d, amount)), hn::Mul(hn::Add(l, r), hn::Set(d, (1.0f - amount) / 2.0f)));
  } else {
    const auto outer = hn::Add(l, r);
    auto result = hn::Zero(d);
    if constexpr (std::is_same_v<hn::TFromD<D>, int16_t>) {
      static_assert(Quantized, "16-bit lanes are only for quantized U8");
      const int t = (half + 256) >> 9;
      const auto center = hn::Mul(c, hn::Set(d, static_cast<int16_t>(t)));
      const auto neighbors = hn::Mul(outer, hn::Set(d, static_cast<int16_t>(64 - t)));
      // Match the old signed saturating sequence; final U8 narrowing clamps.
      return hn::ShiftRight<7>(
          hn::SaturatedAdd(hn::SaturatedAdd(hn::SaturatedAdd(center, neighbors), center), hn::Set(d, int16_t(64))));
    } else if constexpr (Quantized) {
      const int t = (half + 256) >> 9;
      result = hn::ShiftRight<7>(
          hn::Add(hn::Add(hn::Mul(c, hn::Set(d, 2 * t)), hn::Mul(outer, hn::Set(d, 64 - t))), hn::Set(d, 64)));
    } else {
      // Exact 16-bit fixed-point math without 64-bit multiplies. Split half
      // into high/low bytes before multiplying (2*c-l-r). Each intermediate
      // fits int32 even for full-range U16 and half=65536. Arithmetic right
      // shifts implement floor division, including negative sharpening terms.
      const auto delta = hn::Sub(hn::Add(c, c), outer);
      const auto high = hn::Mul(delta, hn::Set(d, half >> 8));
      const auto low =
          hn::Add(hn::Add(hn::Mul(delta, hn::Set(d, half & 255)), hn::ShiftLeft<8>(hn::And(high, hn::Set(d, 255)))),
                  hn::Add(hn::ShiftLeft<15>(hn::And(outer, hn::Set(d, 1))), hn::Set(d, 32768)));
      result = hn::Add(hn::Add(hn::ShiftRight<1>(outer), hn::ShiftRight<8>(high)), hn::ShiftRight<16>(low));
    }
    // Quantized U16 saturates to the storage range in Narrow/DemoteTo.
    if constexpr (Quantized)
      return result;
    else
      return hn::Min(hn::Max(result, hn::Zero(d)), hn::Set(d, peak));
  }
}

template <class T, class D>
HWY_INLINE hn::VFromD<D> Widen(D d, const T* p, size_t count) {
  const hn::Rebind<T, D> dt;
  if constexpr (std::is_same_v<T, float>)
    return Load(dt, p, count);
  else
    return hn::PromoteTo(d, Load(dt, p, count));
}
template <class T, class D>
HWY_INLINE void Narrow(hn::VFromD<D> v, D d, T* p, size_t count) {
  const hn::Rebind<T, D> dt;
  if constexpr (std::is_same_v<T, float>)
    Store(v, dt, p, count);
  else
    Store(hn::DemoteTo(dt, v), dt, p, count);
  (void)d;
}

// Quantized U8 uses 16-bit accumulators; other integer profiles need 32 bits.
template <class T, bool Quantized>
using AdjustAcc = std::conditional_t<std::is_same_v<T, float>, float,
                                     std::conditional_t<Quantized && sizeof(T) == 1, int16_t, int32_t>>;

template <class T, bool Quantized>
HWY_INLINE T AdjustOne(T c, T l, T r, int half, float amount, int peak) {
  if constexpr (std::is_same_v<T, float>)
    return c * amount + (l + r) * ((1.0f - amount) / 2.0f);
  else {
    int64_t value;
    if constexpr (Quantized) {
      const int t = (half + 256) >> 9;
      value = (int64_t(c) * 2 * t + (int64_t(l) + r) * (64 - t) + 64) >> 7;
    } else
      value = (int64_t(c) * 2 * half + (int64_t(l) + r) * (32768 - half) + 32768) >> 16;
    return static_cast<T>(std::clamp(value, int64_t(0), int64_t(peak)));
  }
}

template <class T, bool Quantized>
void VerticalRows(uint8_t* data, int stride, int row, int height, int bits, int half, float amount, uint8_t* scratch) {
  const hn::ScalableTag<AdjustAcc<T, Quantized>> d;
  const size_t n = hn::Lanes(d), width = row / sizeof(T);
  auto* upper = reinterpret_cast<T*>(scratch);
  const int peak = bits == 32 ? 0 : Quantized ? (sizeof(T) == 1 ? 255 : 65535) : (1 << bits) - 1;
  for (int y = 0; y < height; ++y) {
    auto* dst = reinterpret_cast<T*>(data + ptrdiff_t(y) * stride);
    const auto* below = y + 1 == height ? dst : reinterpret_cast<T*>(data + ptrdiff_t(y + 1) * stride);
    size_t x = 0;
#if HWY_ARCH_X86
    if constexpr (Quantized && !std::is_same_v<T, float>) {
      const hn::ScalableTag<T> packed;
      const auto zero = hn::Zero(packed);
      const size_t batch = hn::Lanes(packed);
      for (; x + batch <= width; x += batch) {
        const auto c = hn::LoadU(packed, dst + x);
        const auto l = hn::LoadU(packed, upper + x);
        const auto r = hn::LoadU(packed, below + x);
        hn::StoreU(c, packed, upper + x);
        // x86 unpack/pack share the same 128-bit block order. Keep that
        // order through both arithmetic chains to avoid cross-block shuffles.
        const auto lo = Adjust<true>(d, hn::BitCast(d, hn::InterleaveLower(packed, c, zero)),
                                     hn::BitCast(d, hn::InterleaveLower(packed, l, zero)),
                                     hn::BitCast(d, hn::InterleaveLower(packed, r, zero)), half, amount, peak);
        const auto hi = Adjust<true>(d, hn::BitCast(d, hn::InterleaveUpper(packed, c, zero)),
                                     hn::BitCast(d, hn::InterleaveUpper(packed, l, zero)),
                                     hn::BitCast(d, hn::InterleaveUpper(packed, r, zero)), half, amount, peak);
        hn::StoreU(hn::ReorderDemote2To(packed, lo, hi), packed, dst + x);
      }
    }
#endif
    for (; x + n <= width; x += n) {
      const auto c = Widen(d, dst + x, n);
      const auto result = Adjust<Quantized>(d, c, Widen(d, upper + x, n), Widen(d, below + x, n), half, amount, peak);
      Narrow(c, d, upper + x, n);
      Narrow(result, d, dst + x, n);
    }
    for (; x < width; ++x) {
      const T c = dst[x];
      dst[x] = AdjustOne<T, Quantized>(c, upper[x], below[x], half, amount, peak);
      upper[x] = c;
    }
  }
}
void Vertical(uint8_t* data, int stride, int row, int height, int bits, int half, float amount, uint8_t* scratch,
              uint32_t cpu) {
  const bool quant = vertical_quantized(cpu, row, bits);
  if (bits == 8) {
    if (quant)
      VerticalRows<uint8_t, true>(data, stride, row, height, bits, half, amount, scratch);
    else
      VerticalRows<uint8_t, false>(data, stride, row, height, bits, half, amount, scratch);
  } else if (bits == 32)
    VerticalRows<float, false>(data, stride, row, height, bits, half, amount, scratch);
  else if (quant)
    VerticalRows<uint16_t, true>(data, stride, row, height, bits, half, amount, scratch);
  else
    VerticalRows<uint16_t, false>(data, stride, row, height, bits, half, amount, scratch);
}

template <class T, int Layout, bool Quantized>
void HorizontalSegment(const T* src, T* dst, size_t width, size_t begin, size_t end, int bits, int half, float amount) {
  const hn::ScalableTag<AdjustAcc<T, Quantized>> d;
  const size_t n = hn::Lanes(d);
  constexpr size_t distance = Layout == AIF_FOCUS_RGB3                               ? 3
                              : Layout == AIF_FOCUS_RGB4 || Layout == AIF_FOCUS_YUY2 ? 4
                                                                                     : 1;
  const int peak = bits == 32                                ? 0
                   : Quantized || Layout != AIF_FOCUS_PLANAR ? (sizeof(T) == 1 ? 255 : 65535)
                                                             : (1 << bits) - 1;
  const auto edge = [&](size_t x) HWY_ATTR {
    const size_t step = Layout == AIF_FOCUS_YUY2 ? ((x & 1) ? 4 : 2) : distance;
    dst[x] = AdjustOne<T, Quantized>(src[x], src[x >= step ? x - step : x], src[x + step < width ? x + step : x], half,
                                     amount, peak);
  };
  size_t x = begin;
  for (; x < end && x < distance; ++x)
    edge(x);
  const size_t interior = std::min(end, width > distance ? width - distance : 0);
#if HWY_ARCH_X86
  if constexpr (Quantized && std::is_same_v<T, uint8_t> && Layout != AIF_FOCUS_YUY2) {
    const hn::ScalableTag<uint8_t> packed;
    const auto zero = hn::Zero(packed);
    const size_t batch = hn::Lanes(packed);
    for (; x + batch <= interior; x += batch) {
      const auto c = hn::LoadU(packed, src + x);
      const auto l = hn::LoadU(packed, src + x - distance);
      const auto r = hn::LoadU(packed, src + x + distance);
      const auto lo = Adjust<true>(d, hn::BitCast(d, hn::InterleaveLower(packed, c, zero)),
                                   hn::BitCast(d, hn::InterleaveLower(packed, l, zero)),
                                   hn::BitCast(d, hn::InterleaveLower(packed, r, zero)), half, amount, peak);
      const auto hi = Adjust<true>(d, hn::BitCast(d, hn::InterleaveUpper(packed, c, zero)),
                                   hn::BitCast(d, hn::InterleaveUpper(packed, l, zero)),
                                   hn::BitCast(d, hn::InterleaveUpper(packed, r, zero)), half, amount, peak);
      hn::StoreU(hn::ReorderDemote2To(packed, lo, hi), packed, dst + x);
    }
  }
#endif
  for (; x + n <= interior; x += n) {
    const auto c = Widen(d, src + x, n);
    auto left = Widen(d, src + x - distance, n);
    auto right = Widen(d, src + x + distance, n);
    if constexpr (Layout == AIF_FOCUS_YUY2) {
      using Acc = AdjustAcc<T, Quantized>;
      const auto even = hn::Eq(hn::And(hn::Iota(d, static_cast<Acc>(x & 1)), hn::Set(d, 1)), hn::Zero(d));
      left = hn::IfThenElse(even, Widen(d, src + x - 2, n), left);
      right = hn::IfThenElse(even, Widen(d, src + x + 2, n), right);
    }
    Narrow(Adjust<Quantized>(d, c, left, right, half, amount, peak), d, dst + x, n);
  }
  for (; x < end; ++x)
    edge(x);
}
template <class T, int Layout>
void HorizontalRows(const uint8_t* source, int sp, uint8_t* dest, int dp, int row, int height, int bits, int half,
                    float amount, uint32_t cpu) {
  const size_t width = row / sizeof(T), prefix = horizontal_quantized_prefix(cpu, row, bits, Layout) / sizeof(T);
  for (int y = 0; y < height; ++y) {
    const auto* src = reinterpret_cast<const T*>(source + ptrdiff_t(y) * sp);
    auto* dst = reinterpret_cast<T*>(dest + ptrdiff_t(y) * dp);
    if (prefix)
      HorizontalSegment<T, Layout, true>(src, dst, width, 0, prefix, bits, half, amount);
    if (prefix < width)
      HorizontalSegment<T, Layout, false>(src, dst, width, prefix, width, bits, half, amount);
  }
}
template <class T>
void HorizontalLayout(const uint8_t* src, int sp, uint8_t* dst, int dp, int row, int height, int bits, int layout,
                      int half, float amount, uint32_t cpu) {
  switch (layout) {
    case AIF_FOCUS_PLANAR:
      HorizontalRows<T, AIF_FOCUS_PLANAR>(src, sp, dst, dp, row, height, bits, half, amount, cpu);
      break;
    case AIF_FOCUS_RGB3:
      HorizontalRows<T, AIF_FOCUS_RGB3>(src, sp, dst, dp, row, height, bits, half, amount, cpu);
      break;
    case AIF_FOCUS_RGB4:
      HorizontalRows<T, AIF_FOCUS_RGB4>(src, sp, dst, dp, row, height, bits, half, amount, cpu);
      break;
    case AIF_FOCUS_YUY2:
      HorizontalRows<T, AIF_FOCUS_YUY2>(src, sp, dst, dp, row, height, bits, half, amount, cpu);
      break;
  }
}
void Horizontal(const uint8_t* src, int sp, uint8_t* dst, int dp, int row, int height, int bits, int layout, int half,
                float amount, uint32_t cpu) {
  if (bits == 8)
    HorizontalLayout<uint8_t>(src, sp, dst, dp, row, height, bits, layout, half, amount, cpu);
  else if (bits == 32)
    HorizontalRows<float, AIF_FOCUS_PLANAR>(src, sp, dst, dp, row, height, bits, half, amount, cpu);
  else
    HorizontalLayout<uint16_t>(src, sp, dst, dp, row, height, bits, layout, half, amount, cpu);
}

template <class T>
void TemporalRow(uint8_t* center, const uint8_t* const* neighbors, int count, int row, int bits, int layout,
                 unsigned luma, unsigned chroma, uint32_t cpu) {
  using Acc = std::conditional_t<std::is_same_v<T, float>, float, std::conditional_t<sizeof(T) == 1, int16_t, int32_t>>;
  const hn::ScalableTag<Acc> d;
  const size_t n = hn::Lanes(d), width = row / sizeof(T);
  auto* dst = reinterpret_cast<T*>(center);
  const bool recip16 = temporal_reciprocal16(cpu, row, layout);
  const int divisor15 = 32768 / (count + 1), divisor16 = 65536 / (count + 1);
  const float inverse = 1.0f / static_cast<float>(count + 1);
  for (size_t x = 0; x < width; x += n) {
    const size_t used = std::min(n, width - x);
    const auto c = Widen(d, dst + x, used);
    auto sum = c;
    auto threshold = hn::Set(d, static_cast<Acc>(luma));
    if constexpr (std::is_same_v<T, float>)
      threshold = hn::Set(d, luma / 255.0f);
    else if constexpr (sizeof(T) == 2)
      threshold = hn::Set(d, static_cast<Acc>(luma << (bits - 8)));
    else if (layout == AIF_FOCUS_YUY2) {
      const auto even = hn::Eq(hn::And(hn::Iota(d, static_cast<Acc>(x & 1)), hn::Set(d, 1)), hn::Zero(d));
      threshold = hn::IfThenElse(even, threshold, hn::Set(d, static_cast<Acc>(chroma)));
    }
    for (int i = count - 1; i >= 0; --i) {
      const auto p = Widen(d, reinterpret_cast<const T*>(neighbors[i]) + x, used);
      sum = hn::Add(sum, luma == 255 && layout != AIF_FOCUS_YUY2
                             ? p
                             : hn::IfThenElse(hn::Le(hn::Abs(hn::Sub(c, p)), threshold), p, c));
    }
    if constexpr (std::is_same_v<T, float>)
      sum = hn::Div(sum, hn::Set(d, static_cast<float>(count + 1)));
    else if constexpr (sizeof(T) == 2) {
      const hn::Rebind<float, decltype(d)> df;
      sum = hn::NearestInt(hn::Mul(hn::ConvertTo(df, sum), hn::Set(df, inverse)));
    } else if (recip16) {
      const hn::RebindToUnsigned<decltype(d)> du;
      sum = hn::BitCast(d, hn::ShiftRight<1>(hn::Add(hn::MulHigh(hn::ShiftLeft<1>(hn::BitCast(du, sum)),
                                                                 hn::Set(du, static_cast<uint16_t>(divisor16))),
                                                     hn::Set(du, uint16_t(1)))));
    } else
      sum = hn::MulFixedPoint15(sum, hn::Set(d, static_cast<int16_t>(divisor15)));
    Narrow(sum, d, dst + x, used);
  }
}
void Temporal(uint8_t* c, const uint8_t* const* p, int count, int row, int bits, int layout, unsigned luma,
              unsigned chroma, uint32_t cpu) {
  if (bits == 8)
    TemporalRow<uint8_t>(c, p, count, row, bits, layout, luma, chroma, cpu);
  else if (bits == 32)
    TemporalRow<float>(c, p, count, row, bits, layout, luma, chroma, cpu);
  else
    TemporalRow<uint16_t>(c, p, count, row, bits, layout, luma, chroma, cpu);
}

template <class T>
int64_t SadRows(const uint8_t* a, const uint8_t* b, int sa, int sb, int row, int height, int bits) {
  const hn::ScalableTag<T> d;
  const size_t n = hn::Lanes(d), width = row / sizeof(T);
  int64_t total = 0;
  if constexpr (sizeof(T) == 1) {
    const hn::Repartition<uint64_t, decltype(d)> ds;
    auto sum = hn::Zero(ds);
    for (int y = 0; y < height; ++y) {
      const auto* aa = a + ptrdiff_t(y) * sa;
      const auto* bb = b + ptrdiff_t(y) * sb;
      size_t x = 0;
      for (; x + n <= width; x += n)
        sum = hn::Add(sum, hn::SumsOf8AbsDiff(hn::LoadU(d, aa + x), hn::LoadU(d, bb + x)));
      for (; x < width; ++x)
        total += std::abs(int(aa[x]) - int(bb[x]));
    }
    total += static_cast<int64_t>(hn::GetLane(hn::SumOfLanes(ds, sum)));
  } else {
    const hn::Repartition<uint32_t, decltype(d)> ds;
    for (int y = 0; y < height; ++y) {
      const auto* aa = reinterpret_cast<const T*>(a + ptrdiff_t(y) * sa);
      const auto* bb = reinterpret_cast<const T*>(b + ptrdiff_t(y) * sb);
      size_t x = 0;
      while (x + n <= width) {
        // At most 16384 * 65535 per reduction, including SVE vector widths.
        const size_t end = std::min(width, x + size_t(16384));
        auto sum = hn::Zero(ds);
        for (; x + n <= end; x += n)
          sum = hn::Add(sum, hn::SumsOf2(hn::AbsDiff(hn::LoadU(d, aa + x), hn::LoadU(d, bb + x))));
        total += hn::GetLane(hn::SumOfLanes(ds, sum));
      }
      for (; x < width; ++x)
        total += std::abs(int(aa[x]) - int(bb[x]));
    }
  }
  return total >> (bits - 8);
}
int64_t Sad(const uint8_t* a, const uint8_t* b, int sa, int sb, int row, int height, int bits) {
  return bits == 8 ? SadRows<uint8_t>(a, b, sa, sb, row, height, bits)
                   : SadRows<uint16_t>(a, b, sa, sb, row, height, bits);
}

#endif
} // namespace HWY_NAMESPACE
} // namespace aif::focus
HWY_AFTER_NAMESPACE();
