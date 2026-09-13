// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include <array>
#include <algorithm>
#include "blank_clip/kernel.h"
namespace aif::filters::blank_clip {
inline constexpr uint32_t allowed_cpu_flags(uint64_t flags) {
#if defined(ARM64) || defined(ARM32)
  return flags & CPUF_ARM_NEON ? AIF_BLANK_CLIP_NEON : 0;
#else
  uint32_t cpu = (flags & CPUF_SSE2 ? AIF_BLANK_CLIP_SSE2 : 0) | (flags & CPUF_SSSE3 ? AIF_BLANK_CLIP_SSSE3 : 0);
  const auto sse4 = CPUF_SSE4_1 | CPUF_SSE4_2 | CPUF_AES;
  if ((flags & sse4) != sse4)
    return cpu;
  cpu |= AIF_BLANK_CLIP_SSE4;
  const auto avx2 = CPUF_AVX | CPUF_AVX2 | CPUF_FMA3 | CPUF_F16C;
  if ((flags & avx2) != avx2)
    return cpu;
  cpu |= AIF_BLANK_CLIP_AVX2;
  const auto avx3 = CPUF_AVX512F | CPUF_AVX512CD | CPUF_AVX512BW | CPUF_AVX512DQ | CPUF_AVX512VL;
  if ((flags & avx3) != avx3)
    return cpu;
  cpu |= AIF_BLANK_CLIP_AVX3;
  const auto dl = CPUF_AVX512VNNI | CPUF_AVX512VBMI | CPUF_AVX512VBMI2 | CPUF_AVX512BITALG | CPUF_AVX512VPOPCNTDQ;
  if ((flags & dl) != dl)
    return cpu;
  cpu |= AIF_BLANK_CLIP_AVX3_DL;
  if (flags & CPUF_AVX512BF16) {
    cpu |= AIF_BLANK_CLIP_AVX3_ZEN4;
    if (flags & CPUF_AVX512FP16)
      cpu |= AIF_BLANK_CLIP_AVX3_SPR;
  }
  return cpu;
#endif
}
inline uint32_t allowed_cpu(IScriptEnvironment* env) {
  return allowed_cpu_flags(env->GetCPUFlagsEx());
}
inline std::uint32_t rgb_to_yuv_rec601(const std::uint32_t rgb) noexcept {
  constexpr int cyb = static_cast<int>(0.114 * 219 / 255 * 65536 + 0.5);
  constexpr int cyg = static_cast<int>(0.587 * 219 / 255 * 65536 + 0.5);
  constexpr int cyr = static_cast<int>(0.299 * 219 / 255 * 65536 + 0.5);
  constexpr int scaled_y_coefficient = static_cast<int>(255.0 / 219.0 * 65536 + 0.5);
  constexpr int u_coefficient = static_cast<int>(1.0 / 2.018 * 1024 + 0.5);
  constexpr int v_coefficient = static_cast<int>(1.0 / 1.596 * 1024 + 0.5);

  const int blue = static_cast<int>(rgb & 0xFFU);
  const int green = static_cast<int>((rgb >> 8U) & 0xFFU);
  const int red = static_cast<int>((rgb >> 16U) & 0xFFU);
  const int y = (cyb * blue + cyg * green + cyr * red + 0x108000) >> 16;
  const int scaled_y = (y - 16) * scaled_y_coefficient;
  const int u = std::clamp(((((blue << 16) - scaled_y) >> 10) * u_coefficient + 0x800000 + 32768) >> 16, 0, 255);
  const int v = std::clamp(((((red << 16) - scaled_y) >> 10) * v_coefficient + 0x800000 + 32768) >> 16, 0, 255);
  return static_cast<std::uint32_t>((y * 256 + u) * 256 + v) | (rgb & 0xFF000000U);
}

struct FillTarget {
  PVideoFrame& frame;
  int plane;
  IScriptEnvironment* env;
};
template <class T, size_t N>
void fill_pattern(FillTarget d, const std::array<T, N>& pattern) {
  if (aif_blank_clip_fill(d.frame->GetWritePtr(d.plane), d.frame->GetPitch(d.plane), d.frame->GetRowSize(d.plane),
                          d.frame->GetHeight(d.plane), pattern.data(), sizeof(T) * N, allowed_cpu(d.env)))
    d.env->ThrowError("BlankClip: invalid fill geometry");
}
inline void fill_blank_u8(FillTarget d, uint8_t v) {
  fill_pattern(d, std::array<uint8_t, 1>{v});
}
inline void fill_blank_u16(FillTarget d, uint16_t v) {
  fill_pattern(d, std::array<uint16_t, 1>{v});
}
inline void fill_blank_f32(FillTarget d, float v) {
  fill_pattern(d, std::array<float, 1>{v});
}
inline void fill_blank_yuy2(FillTarget d, uint8_t y, uint8_t u, uint8_t v) {
  fill_pattern(d, std::array<uint8_t, 4>{y, u, y, v});
}
inline void fill_blank_bgr24(FillTarget d, uint8_t b, uint8_t g, uint8_t r) {
  fill_pattern(d, std::array<uint8_t, 3>{b, g, r});
}
inline void fill_blank_bgr32(FillTarget d, uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
  fill_pattern(d, std::array<uint8_t, 4>{b, g, r, a});
}
inline void fill_blank_bgr48(FillTarget d, uint16_t b, uint16_t g, uint16_t r) {
  fill_pattern(d, std::array<uint16_t, 3>{b, g, r});
}
inline void fill_blank_bgr64(FillTarget d, uint16_t b, uint16_t g, uint16_t r, uint16_t a) {
  fill_pattern(d, std::array<uint16_t, 4>{b, g, r, a});
}
} // namespace aif::filters::blank_clip
