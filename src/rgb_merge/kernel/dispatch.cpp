// SPDX-License-Identifier: GPL-2.0-or-later
#include "rgb_merge/kernel.h"
#include <video_convert/layout.h>
#ifndef AIF_SCALAR_ONLY
#include <hwy/detect_targets.h>
#endif
#include <vector>
#include <cstring>
#include <limits>
extern "C" uint32_t aif_rgb_merge_supported_cpu(void) {
  uint32_t cpu = 0;
#ifndef AIF_SCALAR_ONLY
  const auto t = vc_layout_supported_targets();
  if (t & HWY_SSE2)
    cpu |= AIF_RGB_MERGE_SSE2;
  if (t & HWY_SSSE3)
    cpu |= AIF_RGB_MERGE_SSSE3;
  if (t & HWY_NEON_WITHOUT_AES)
    cpu |= AIF_RGB_MERGE_NEON;
  if (t & HWY_SSE4)
    cpu |= AIF_RGB_MERGE_SSE4;
  if (t & HWY_AVX2)
    cpu |= AIF_RGB_MERGE_AVX2;
  if (t & HWY_AVX3)
    cpu |= AIF_RGB_MERGE_AVX3;
  if (t & HWY_AVX3_DL)
    cpu |= AIF_RGB_MERGE_AVX3_DL;
  if (t & HWY_AVX3_ZEN4)
    cpu |= AIF_RGB_MERGE_AVX3_ZEN4;
  if (t & HWY_AVX3_SPR)
    cpu |= AIF_RGB_MERGE_AVX3_SPR;
  if (t & HWY_AVX10_2)
    cpu |= AIF_RGB_MERGE_AVX10_2;
#endif
  return cpu;
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
  if (cpu & AIF_RGB_MERGE_SSE4)
    mask |= HWY_SSE4;
  if (cpu & AIF_RGB_MERGE_AVX2)
    mask |= HWY_AVX2;
  if (cpu & AIF_RGB_MERGE_AVX3)
    mask |= HWY_AVX3;
  if (cpu & AIF_RGB_MERGE_AVX3_DL)
    mask |= HWY_AVX3_DL;
  if (cpu & AIF_RGB_MERGE_AVX3_ZEN4)
    mask |= HWY_AVX3_ZEN4;
  if (cpu & AIF_RGB_MERGE_AVX3_SPR)
    mask |= HWY_AVX3_SPR;
  if (cpu & AIF_RGB_MERGE_AVX10_2)
    mask |= HWY_AVX10_2;
  mask &= vc_layout_supported_targets();
#else
  (void)cpu;
#endif
  return vc_get_layout_functions(mask & -mask);
}
// Restrict the measured Zen4 fallback to BGRA packing; keep unpacking targets.
const vc_layout_functions* pack_layout(uint32_t cpu, int bytes, int dc, int w, int h) {
  const auto available = cpu & aif_rgb_merge_supported_cpu();
  const uint32_t higher = AIF_RGB_MERGE_AVX3_ZEN4 | AIF_RGB_MERGE_AVX3_SPR | AIF_RGB_MERGE_AVX10_2;
  if (dc == 4 && uint64_t(w) * h >= 1920u * 1080u && (available & higher) == AIF_RGB_MERGE_AVX3_ZEN4) {
    const uint32_t target = bytes == 1 ? AIF_RGB_MERGE_AVX3 : AIF_RGB_MERGE_AVX3_DL;
    if (aif_rgb_merge_supported_cpu() & target)
      return layout(target);
  }
  return layout(cpu);
}
} // namespace
extern "C" int aif_rgb_merge_render(const uint8_t* const src[4], const int sp[4], const int kinds[4],
                                    uint8_t* const dst[4], const int dp[4], int w, int h, int bytes, int dc,
                                    uint32_t cpu) {
  if (!src || !sp || !kinds || !dst || !dp || w <= 0 || h <= 0 || (bytes != 1 && bytes != 2 && bytes != 4) ||
      (dc != 1 && dc != 3 && dc != 4) || w > std::numeric_limits<int>::max() / bytes / 4 || (bytes == 4 && dc != 1))
    return VC_INVALID_ARGUMENT;
  int rb = w * bytes;
  auto valid = [&](const void* p, int pitch, int row) {
    return p && pitch >= row && pitch % bytes == 0 && reinterpret_cast<uintptr_t>(p) % bytes == 0;
  };
  for (int c = 0; c < 4; ++c) {
    if (c < 3 || src[c]) {
      int k = kinds[c];
      if (k < 1 || k > 4 || (k == 2 && (bytes != 1 || (w & 1))) || (bytes == 4 && k != 1) || (c == 3 && k == 3) ||
          !valid(src[c], sp[c], rb * k))
        return VC_INVALID_ARGUMENT;
    }
    if (dc == 1) {
      if ((c < 3 || dst[c]) && !valid(dst[c], dp[c], rb))
        return VC_INVALID_ARGUMENT;
    }
  }
  if (dc != 1 && !valid(dst[0], dp[0], rb * dc))
    return VC_INVALID_ARGUMENT;
  if (dc != 1 && kinds[0] == 1 && kinds[1] == 1 && kinds[2] == 1 && (!src[3] || kinds[3] == 1)) {
    // All source planes are already available: pack the frame in one validated call.
    const vc_const_rgb_planes planes{{src[0], sp[0]}, {src[1], sp[1]}, {src[2], sp[2]}, {src[3], sp[3]}};
    return pack_layout(cpu, bytes, dc, w, h)
        ->pack_bgr(planes, {dst[0] + ptrdiff_t(h - 1) * dp[0], -ptrdiff_t(dp[0])}, bytes == 1 ? VC_U8 : VC_U16, dc, 0,
                   {w, h, 0, h});
  }
  try {
    auto fn = layout(cpu);
    const auto available = cpu & aif_rgb_merge_supported_cpu();
    const uint32_t wide = AIF_RGB_MERGE_AVX3 | AIF_RGB_MERGE_AVX3_DL | AIF_RGB_MERGE_AVX3_ZEN4 |
                          AIF_RGB_MERGE_AVX3_SPR | AIF_RGB_MERGE_AVX10_2;
    const bool avx2_available = (aif_rgb_merge_supported_cpu() & AIF_RGB_MERGE_AVX2) != 0;
    const bool rgb24_avx3 = bytes == 1 && (available & wide) == AIF_RGB_MERGE_AVX3 && avx2_available;
    const bool rgb48_small =
        bytes == 2 && w <= 640 && !(available & (AIF_RGB_MERGE_AVX3_SPR | AIF_RGB_MERGE_AVX10_2)) &&
        (available & (AIF_RGB_MERGE_AVX3 | AIF_RGB_MERGE_AVX3_DL | AIF_RGB_MERGE_AVX3_ZEN4)) && avx2_available;
    const bool measured_wide = (available & wide) && !(available & (AIF_RGB_MERGE_AVX3_SPR | AIF_RGB_MERGE_AVX10_2));
    const auto* luma = measured_wide && avx2_available ? layout(AIF_RGB_MERGE_AVX2) : fn;
    const auto* unpack3 = rgb24_avx3 || rgb48_small ? layout(AIF_RGB_MERGE_AVX2) : fn;
    const auto* unpack4 = rgb24_avx3 && w <= 640 ? layout(AIF_RGB_MERGE_AVX2) : fn;
    const auto* pack = rgb24_avx3 && dc == 3 ? layout(AIF_RGB_MERGE_AVX2) : pack_layout(cpu, bytes, dc, w, h);
    size_t words = (size_t(rb) + 3) / 4;
    std::vector<uint32_t> scratch(words * 8, 0);
    vc_rows rows{w, 1, 0, 1};
    for (int y = 0; y < h; ++y) {
      vc_const_plane p[4] = {};
      for (int c = 0; c < 4; ++c) {
        if (!src[c])
          continue;
        int k = kinds[c];
        vc_const_plane s{src[c] + ptrdiff_t(k >= 3 ? h - 1 - y : y) * sp[c], sp[c]};
        if (k == 1)
          p[c] = s;
        else if (k == 2) {
          vc_plane d{scratch.data() + words * c, rb};
          int status = luma->extract_yuy2_luma(s, d, rows);
          if (status)
            return status;
          p[c] = {d.data, rb};
        } else {
          vc_plane t[4];
          for (int i = 0; i < 4; ++i)
            t[i] = {scratch.data() + words * (i == c ? c : 4 + i), rb};
          int status = (k == 3 ? unpack3 : unpack4)
                           ->unpack_bgr(s, {t[0], t[1], t[2], t[3]}, bytes == 1 ? VC_U8 : VC_U16, k, 0, rows);
          if (status)
            return status;
          p[c] = {t[c].data, rb};
        }
      }
      if (dc == 1) {
        for (int c = 0; c < 4; ++c)
          if (dst[c]) {
            auto d = dst[c] + ptrdiff_t(y) * dp[c];
            if (p[c].data)
              std::memcpy(d, p[c].data, rb);
            else
              std::memset(d, 0, rb);
          }
      } else {
        int status = pack->pack_bgr({p[0], p[1], p[2], p[3]}, {dst[0] + ptrdiff_t(h - 1 - y) * dp[0], dp[0]},
                                    bytes == 1 ? VC_U8 : VC_U16, dc, 0, rows);
        if (status)
          return status;
      }
    }
    return VC_OK;
  } catch (...) {
    return VC_OUT_OF_MEMORY;
  }
}
