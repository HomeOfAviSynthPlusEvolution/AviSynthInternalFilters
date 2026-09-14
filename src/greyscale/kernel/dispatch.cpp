// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <video_convert/matrix.h>
#ifndef AIF_SCALAR_ONLY
#include <hwy/detect_targets.h>
#endif
#include <cstring>
#include <limits>
#include <memory>
#include <vector>
struct aif_greyscale_plan {
  vc_matrix_plan* matrix = nullptr;
  const vc_layout_functions* layout = nullptr;
  vc_matrix_plan* packed_matrix = nullptr;
  const vc_layout_functions* packed_layout = nullptr;
  int fallback_components = 0;
  int bytes = 0;
  aif::greyscale::FloatRgb float_rgb = nullptr;
  float kr = 0, kg = 0, kb = 0;
  ~aif_greyscale_plan() {
    vc_matrix_destroy(matrix);
    vc_matrix_destroy(packed_matrix);
  }
};
namespace {
int64_t target(uint32_t cpu, int64_t supported) {
#ifdef AIF_SCALAR_ONLY
  (void)cpu;
  (void)supported;
  return 0;
#else
  int64_t mask = 0;
  if (cpu & AIF_GREYSCALE_SSE2)
    mask |= HWY_SSE2;
  if (cpu & AIF_GREYSCALE_SSSE3)
    mask |= HWY_SSSE3;
  if (cpu & AIF_GREYSCALE_SSE4)
    mask |= HWY_SSE4;
  if (cpu & AIF_GREYSCALE_AVX2)
    mask |= HWY_AVX2;
  if (cpu & AIF_GREYSCALE_AVX3)
    mask |= HWY_AVX3;
  if (cpu & AIF_GREYSCALE_AVX3_DL)
    mask |= HWY_AVX3_DL;
  if (cpu & AIF_GREYSCALE_AVX3_ZEN4)
    mask |= HWY_AVX3_ZEN4;
  if (cpu & AIF_GREYSCALE_AVX3_SPR)
    mask |= HWY_AVX3_SPR;
  if (cpu & AIF_GREYSCALE_AVX10_2)
    mask |= HWY_AVX10_2;
  if (cpu & AIF_GREYSCALE_NEON)
    mask |= HWY_NEON_WITHOUT_AES;
  mask &= supported;
  return mask & -mask;
#endif
}
} // namespace
extern "C" int aif_greyscale_create(double kr, double kb, int bits, int sf, int df, uint32_t cpu,
                                    aif_greyscale_plan** out) {
  if (!out)
    return VC_INVALID_ARGUMENT;
  *out = nullptr;
  try {
    auto p = std::make_unique<aif_greyscale_plan>();
    vc_matrix_config config{kr, kb, bits, 15, sf, df, VC_RGB_TO_Y};
    int status = vc_matrix_create_for_target(&config, target(cpu, vc_matrix_supported_targets()), &p->matrix);
    if (status)
      return status;
    p->layout = vc_get_layout_functions(target(cpu, vc_layout_supported_targets()));
#ifndef AIF_SCALAR_ONLY
    // Repeated packed-format measurements: SSE4 RGB24 favors SSSE3, and
    // AVX3 RGB32 favors AVX2. Other formats keep the requested target.
    const auto selected = target(cpu, vc_matrix_supported_targets() & vc_layout_supported_targets());
    int64_t fallback = 0;
    if (bits == 8 && selected == HWY_SSE4) {
      fallback = HWY_SSSE3;
      p->fallback_components = 3;
    } else if (bits == 8 && selected == HWY_AVX3) {
      fallback = HWY_AVX2;
      p->fallback_components = 4;
    }
    if (fallback && (fallback & vc_matrix_supported_targets() & vc_layout_supported_targets())) {
      status = vc_matrix_create_for_target(&config, fallback, &p->packed_matrix);
      if (status)
        return status;
      p->packed_layout = vc_get_layout_functions(fallback);
    }
#endif
    p->bytes = bits == 8 ? 1 : bits == 32 ? 4 : 2;
    if (bits == 32 && sf && df) {
      if (const auto* kernels = aif::greyscale::backend(cpu))
        p->float_rgb = kernels->float_rgb;
      p->kr = float(kr);
      p->kg = float(1.0 - kr - kb);
      p->kb = float(kb);
    }
    *out = p.release();
    return VC_OK;
  } catch (...) {
    return VC_OUT_OF_MEMORY;
  }
}
extern "C" void aif_greyscale_destroy(aif_greyscale_plan* p) {
  delete p;
}
extern "C" int aif_greyscale_rgb(const aif_greyscale_plan* p, uint8_t* const data[3], const int pitch[3], int w, int h,
                                 int packed, int components) {
  if (!p || !data || !pitch || w <= 0 || h <= 0 || (packed != 0 && packed != 1) || (components != 3 && components != 4))
    return VC_INVALID_ARGUMENT;
  if (w > std::numeric_limits<int>::max() / p->bytes / components || (packed && p->bytes == 4))
    return VC_INVALID_ARGUMENT;
  const int rb = w * p->bytes;
  for (int i = 0; i < (packed ? 1 : 3); ++i)
    if (!data[i] || pitch[i] < rb * (packed ? components : 1) || pitch[i] % p->bytes ||
        reinterpret_cast<uintptr_t>(data[i]) % p->bytes)
      return VC_INVALID_ARGUMENT;
  if (!packed && p->float_rgb) {
    p->float_rgb(data, pitch, w, h, p->kr, p->kg, p->kb);
    return VC_OK;
  }
  const bool use_packed = packed && components == p->fallback_components && p->packed_matrix;
  const auto* matrix = use_packed ? p->packed_matrix : p->matrix;
  const auto* layout = use_packed ? p->packed_layout : p->layout;
  try {
    const size_t words = (size_t(rb) + 3) / 4;
    std::vector<uint32_t> scratch(packed ? words * 5 : 0);
    vc_plane yplane{scratch.data(), rb};
    const vc_rows rows{w, 1, 0, 1};
    for (int y = 0; y < h; ++y) {
      vc_const_rgb_planes rgb{};
      vc_plane a{};
      if (packed) {
        vc_plane r{scratch.data() + words, rb}, g{scratch.data() + words * 2, rb}, b{scratch.data() + words * 3, rb};
        a = {scratch.data() + words * 4, rb};
        int s = layout->unpack_bgr({data[0] + ptrdiff_t(y) * pitch[0], pitch[0]}, {r, g, b, a},
                                   p->bytes == 1 ? VC_U8 : VC_U16, components, 0, rows);
        if (s)
          return s;
        rgb = {{r.data, r.stride}, {g.data, g.stride}, {b.data, b.stride}, {}};
      } else {
        yplane = {data[0] + ptrdiff_t(y) * pitch[0], pitch[0]};
        rgb = {{data[0] + ptrdiff_t(y) * pitch[0], pitch[0]},
               {data[1] + ptrdiff_t(y) * pitch[1], pitch[1]},
               {data[2] + ptrdiff_t(y) * pitch[2], pitch[2]},
               {}};
      }
      int s = vc_matrix_rgb_to_y(matrix, rgb, yplane, rows);
      if (s)
        return s;
      if (packed) {
        vc_const_plane l{yplane.data, yplane.stride};
        s = layout->pack_bgr({l, l, l, {a.data, a.stride}}, {data[0] + ptrdiff_t(y) * pitch[0], pitch[0]},
                             p->bytes == 1 ? VC_U8 : VC_U16, components, 0, rows);
        if (s)
          return s;
      } else
        for (int c = 1; c < 3; ++c)
          std::memcpy(data[c] + ptrdiff_t(y) * pitch[c], yplane.data, rb);
    }
    return VC_OK;
  } catch (...) {
    return VC_OUT_OF_MEMORY;
  }
}
extern "C" int aif_greyscale_yuy2(uint8_t* data, int pitch, int w, int h, uint32_t cpu) {
  return vc_get_layout_functions(target(cpu, vc_layout_supported_targets()))
      ->neutralize_yuy2_chroma({data, pitch}, {w, h, 0, h});
}
extern "C" int aif_greyscale_chroma(uint8_t* data, int pitch, int w, int h, int bits, uint32_t cpu) {
  if (bits != 8 && bits != 10 && bits != 12 && bits != 14 && bits != 16 && bits != 32)
    return VC_INVALID_ARGUMENT;
  int bytes = bits == 8 ? 1 : bits == 32 ? 4 : 2;
  if (!data || w <= 0 || h <= 0 || w > std::numeric_limits<int>::max() / bytes || pitch < w * bytes || pitch % bytes ||
      reinterpret_cast<uintptr_t>(data) % bytes)
    return VC_INVALID_ARGUMENT;
  uint32_t value = bits == 32 ? 0 : 1u << (bits - 1);
  const auto* kernels = aif::greyscale::backend(cpu);
  auto fill = kernels ? kernels->fill : nullptr;
  if (!fill)
    fill = aif::greyscale::scalar;
  for (int y = 0; y < h; ++y)
    fill(data + ptrdiff_t(y) * pitch, w * bytes, reinterpret_cast<const uint8_t*>(&value), bytes);
  return VC_OK;
}
