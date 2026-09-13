// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <video_convert/layout.h>
#ifndef AIF_SCALAR_ONLY
#include <hwy/detect_targets.h>
#endif
#include <vector>
#include <cstring>
#include <limits>
extern "C" uint32_t aif_channel_display_supported_cpu(void) {
  uint32_t cpu = 0;
#ifndef AIF_SCALAR_ONLY
  const auto t = vc_layout_supported_targets();
  if (t & HWY_SSE2)
    cpu |= AIF_CHANNEL_DISPLAY_SSE2;
  if (t & HWY_SSSE3)
    cpu |= AIF_CHANNEL_DISPLAY_SSSE3;
  if (t & HWY_NEON_WITHOUT_AES)
    cpu |= AIF_CHANNEL_DISPLAY_NEON;
  if (t & HWY_SSE4)
    cpu |= AIF_CHANNEL_DISPLAY_SSE4;
  if (t & HWY_AVX2)
    cpu |= AIF_CHANNEL_DISPLAY_AVX2;
  if (t & HWY_AVX3)
    cpu |= AIF_CHANNEL_DISPLAY_AVX3;
  if (t & HWY_AVX3_DL)
    cpu |= AIF_CHANNEL_DISPLAY_AVX3_DL;
  if (t & HWY_AVX3_ZEN4)
    cpu |= AIF_CHANNEL_DISPLAY_AVX3_ZEN4;
  if (t & HWY_AVX3_SPR)
    cpu |= AIF_CHANNEL_DISPLAY_AVX3_SPR;
  if (t & HWY_AVX10_2)
    cpu |= AIF_CHANNEL_DISPLAY_AVX10_2;
#endif
  return cpu & aif::channel_display::packed_supported_cpu();
}
namespace {
const vc_layout_functions* layout(uint32_t cpu) {
  int64_t mask = 0;
#ifndef AIF_SCALAR_ONLY
  if (cpu & 1)
    mask |= HWY_SSE2;
  if (cpu & 2)
    mask |= HWY_SSSE3;
  if (cpu & 4)
    mask |= HWY_NEON_WITHOUT_AES;
  if (cpu & AIF_CHANNEL_DISPLAY_SSE4)
    mask |= HWY_SSE4;
  if (cpu & AIF_CHANNEL_DISPLAY_AVX2)
    mask |= HWY_AVX2;
  if (cpu & AIF_CHANNEL_DISPLAY_AVX3)
    mask |= HWY_AVX3;
  if (cpu & AIF_CHANNEL_DISPLAY_AVX3_DL)
    mask |= HWY_AVX3_DL;
  if (cpu & AIF_CHANNEL_DISPLAY_AVX3_ZEN4)
    mask |= HWY_AVX3_ZEN4;
  if (cpu & AIF_CHANNEL_DISPLAY_AVX3_SPR)
    mask |= HWY_AVX3_SPR;
  if (cpu & AIF_CHANNEL_DISPLAY_AVX10_2)
    mask |= HWY_AVX10_2;
  mask &= vc_layout_supported_targets();
#else
  (void)cpu;
#endif
  return vc_get_layout_functions(mask & -mask);
}
} // namespace
extern "C" int aif_channel_display_render(const uint8_t* src, int sp, const uint8_t* alpha, int ap,
                                          uint8_t* const dst[4], const int dp[4], int w, int h, int bytes, int sc,
                                          int dc, int channel, uint32_t cpu) {
  if (!src || !dst || !dp || !dst[0] || w <= 0 || h <= 0 || (bytes != 1 && bytes != 2) ||
      (sc != 1 && sc != 3 && sc != 4) || dc < 1 || dc > 4 || channel < 0 || channel >= sc ||
      w > std::numeric_limits<int>::max() / bytes / 4 || (dc == 2 && (bytes != 1 || (w & 1))))
    return VC_INVALID_ARGUMENT;
  int rb = w * bytes;
  auto valid = [&](const void* p, int stride, int row) {
    return p && stride >= row && stride % bytes == 0 && reinterpret_cast<uintptr_t>(p) % bytes == 0;
  };
  if (!valid(src, sp, rb * sc) || (sc == 1 && alpha && !valid(alpha, ap, rb)))
    return VC_INVALID_ARGUMENT;
  for (int c = 0; c < (dc == 1 ? 4 : 1); ++c)
    if (dst[c] && !valid(dst[c], dp[c], rb * (dc == 1 ? 1 : dc)))
      return VC_INVALID_ARGUMENT;
  if (sc >= 3 && dc >= 3) {
    if (auto render = aif::channel_display::backend(cpu)) {
      for (int y = 0; y < h; ++y)
        render(src + ptrdiff_t(y) * sp, dst[0] + ptrdiff_t(y) * dp[0], w, bytes, sc, dc, channel);
      return VC_OK;
    }
  }
  try {
    const auto* fn = layout(cpu);
    // On the measured AVX512 profiles, neutral-chroma YUY2 packing favors AVX2.
    const uint32_t measured_wide =
        AIF_CHANNEL_DISPLAY_AVX3 | AIF_CHANNEL_DISPLAY_AVX3_DL | AIF_CHANNEL_DISPLAY_AVX3_ZEN4;
    const auto supported = aif_channel_display_supported_cpu();
    const auto available = cpu & supported;
    const auto* yuy2 = dc == 2 && (available & measured_wide) &&
                               !(available & (AIF_CHANNEL_DISPLAY_AVX3_SPR | AIF_CHANNEL_DISPLAY_AVX10_2)) &&
                               (supported & AIF_CHANNEL_DISPLAY_AVX2)
                           ? layout(AIF_CHANNEL_DISPLAY_AVX2)
                           : fn;
    size_t words = (size_t(rb) + 3) / 4;
    std::vector<uint32_t> scratch(words * 5, bytes == 1 ? 0x80808080u : 0u);
    int storage = bytes == 1 ? VC_U8 : VC_U16;
    uint32_t opaque = bytes == 1 ? 255 : 65535;
    vc_rows rows{w, 1, 0, 1};
    for (int y = 0; y < h; ++y) {
      vc_const_plane selected{src + ptrdiff_t(sc == 1 ? y : h - 1 - y) * sp, sp};
      vc_const_plane a{alpha ? alpha + ptrdiff_t(y) * ap : nullptr, ap};
      if (sc != 1) {
        vc_plane r{scratch.data(), rb}, g{scratch.data() + words, rb}, b{scratch.data() + words * 2, rb},
            aa{scratch.data() + words * 3, rb};
        int s = fn->unpack_bgr(selected, {r, g, b, aa}, storage, sc, opaque, rows);
        if (s)
          return s;
        vc_plane p[] = {b, g, r, aa};
        selected = {p[channel].data, rb};
        a = {aa.data, rb};
      }
      if (dc >= 3) {
        int s = fn->pack_bgr({selected, selected, selected, a}, {dst[0] + ptrdiff_t(h - 1 - y) * dp[0], dp[0]}, storage,
                             dc, opaque, rows);
        if (s)
          return s;
      } else if (dc == 2) {
        vc_const_plane uv{scratch.data() + words * 4, w / 2};
        int s = yuy2->pack_yuy2({selected, uv, uv}, {dst[0] + ptrdiff_t(y) * dp[0], dp[0]}, rows);
        if (s)
          return s;
      } else {
        for (int c = 0; c < 3; ++c)
          if (dst[c])
            std::memcpy(dst[c] + ptrdiff_t(y) * dp[c], selected.data, rb);
        if (dst[3]) {
          if (a.data)
            std::memcpy(dst[3] + ptrdiff_t(y) * dp[3], a.data, rb);
          else
            std::memset(dst[3] + ptrdiff_t(y) * dp[3], 255, rb);
        }
      }
    }
    return VC_OK;
  } catch (...) {
    return VC_OUT_OF_MEMORY;
  }
}
