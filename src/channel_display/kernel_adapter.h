// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include "../common/host_cpu.h"
#include "channel_display/kernel.h"
#include <blank_clip/kernel.h>
#include <array>
#include <cctype>
namespace aif::filters::channel_display {
inline int lstrcmpi(const char* a, const char* b) {
  for (; *a && *b; ++a, ++b) {
    int d = std::tolower((unsigned char)*a) - std::tolower((unsigned char)*b);
    if (d)
      return d;
  }
  return *a - *b;
}
inline int pixel_type_id(const char* name, IScriptEnvironment* env) {
  try {
    AVSValue args[] = {16, 16, 1, name};
    const char* names[] = {"width", "height", "length", "pixel_type"};
    return env->Invoke("BlankClip", AVSValue(args, 4), names).AsClip()->GetVideoInfo().pixel_type;
  } catch (const AvisynthError&) {
    return VideoInfo::CS_UNKNOWN;
  }
}
inline constexpr uint32_t allowed_cpu_flags(uint64_t flags) {
#if defined(ARM64) || defined(ARM32)
  return flags & CPUF_ARM_NEON ? AIF_CHANNEL_DISPLAY_NEON : 0;
#else
  uint32_t cpu =
      (flags & CPUF_SSE2 ? AIF_CHANNEL_DISPLAY_SSE2 : 0) | (flags & CPUF_SSSE3 ? AIF_CHANNEL_DISPLAY_SSSE3 : 0);
  const auto sse4 = CPUF_SSE4_1 | CPUF_SSE4_2 | CPUF_AES;
  if ((flags & sse4) != sse4)
    return cpu;
  cpu |= AIF_CHANNEL_DISPLAY_SSE4;
  const auto avx2 = CPUF_AVX | CPUF_AVX2 | CPUF_FMA3 | CPUF_F16C;
  if ((flags & avx2) != avx2)
    return cpu;
  cpu |= AIF_CHANNEL_DISPLAY_AVX2;
  const auto avx3 = CPUF_AVX512F | CPUF_AVX512CD | CPUF_AVX512BW | CPUF_AVX512DQ | CPUF_AVX512VL;
  if ((flags & avx3) != avx3)
    return cpu;
  cpu |= AIF_CHANNEL_DISPLAY_AVX3;
  const auto dl = CPUF_AVX512VNNI | CPUF_AVX512VBMI | CPUF_AVX512VBMI2 | CPUF_AVX512BITALG | CPUF_AVX512VPOPCNTDQ;
  if ((flags & dl) != dl)
    return cpu;
  cpu |= AIF_CHANNEL_DISPLAY_AVX3_DL;
  if (flags & CPUF_AVX512BF16) {
    cpu |= AIF_CHANNEL_DISPLAY_AVX3_ZEN4;
    if (flags & CPUF_AVX512FP16)
      cpu |= AIF_CHANNEL_DISPLAY_AVX3_SPR;
  }
  return cpu;
#endif
}
inline uint32_t cpu(IScriptEnvironment* env) {
  return allowed_cpu_flags(aif::filters::host_cpu_flags(env));
}
inline void render(const uint8_t* s, int sp, const uint8_t* a, int ap, std::array<uint8_t*, 4> d, std::array<int, 4> dp,
                   int w, int h, int b, int sc, int dc, int c, IScriptEnvironment* env, uint32_t cpu_mask) {
  if (aif_channel_display_render(s, sp, a, ap, d.data(), dp.data(), w, h, b, sc, dc, c, cpu_mask))
    env->ThrowError("ShowChannel: layout failed");
}
template <class T>
void fill_plane(uint8_t* p, int h, int row, int pitch, T v, IScriptEnvironment* env, uint32_t cpu_mask) {
  if (aif_blank_clip_fill(p, pitch, row, h, &v, sizeof(T), cpu_mask))
    env->ThrowError("ShowChannel: fill failed");
}
template <class T>
void fill_chroma(uint8_t* u, uint8_t* v, int h, int row, int pitch, T value, IScriptEnvironment* env,
                 uint32_t cpu_mask) {
  fill_plane(u, h, row, pitch, value, env, cpu_mask);
  fill_plane(v, h, row, pitch, value, env, cpu_mask);
}
template <class T, bool SA, bool DA>
void planar_to_packedrgb(uint8_t* d, int dp, const uint8_t* s, const uint8_t* a, int sp, int w, int h,
                         IScriptEnvironment* env, uint32_t cpu_mask) {
  render(s, sp, SA ? a : nullptr, sp, {d, nullptr, nullptr, nullptr}, {dp, 0, 0, 0}, w, h, sizeof(T), 1, DA ? 4 : 3, 0,
         env, cpu_mask);
}
template <class T, bool SA, bool DA>
void packed_to_packedrgb(uint8_t* d, int dp, const uint8_t* s, int sp, int w, int h, int c, IScriptEnvironment* env,
                         uint32_t cpu_mask) {
  render(s, sp, nullptr, 0, {d, nullptr, nullptr, nullptr}, {dp, 0, 0, 0}, w, h, sizeof(T), SA ? 4 : 3, DA ? 4 : 3, c,
         env, cpu_mask);
}
template <class T, bool SA, bool DA>
void packed_to_planarrgb(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a, int dp, const uint8_t* s, int sp, int w, int h,
                         int c, IScriptEnvironment* env, uint32_t cpu_mask) {
  render(s, sp, nullptr, 0, {r, g, b, DA ? a : nullptr}, {dp, dp, dp, dp}, w, h, sizeof(T), SA ? 4 : 3, 1, c, env,
         cpu_mask);
}
template <class T, bool SA, bool DA>
void packed_to_luma_alpha(uint8_t* d, uint8_t* a, int dp, const uint8_t* s, int sp, int w, int h, int c,
                          IScriptEnvironment* env, uint32_t cpu_mask) {
  render(s, sp, nullptr, 0, {d, nullptr, nullptr, DA ? a : nullptr}, {dp, 0, 0, dp}, w, h, sizeof(T), SA ? 4 : 3, 1, c,
         env, cpu_mask);
}
} // namespace aif::filters::channel_display
