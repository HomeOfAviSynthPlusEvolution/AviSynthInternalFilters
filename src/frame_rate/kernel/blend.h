#pragma once
#include <avisynth.h>
#include <composite/dispatch.h>
namespace aif::filters::frame_rate {
inline void blend_scanline(BYTE* dst, const BYTE* a, const BYTE* b, int row, int64_t scale, int zone) {
  for (int x = 0; x < row; ++x)
    dst[x] = BYTE(a[x] + ((b[x] - a[x]) * scale + (zone >> 1)) / zone);
}
inline void MergePlane(const cp_kernels* kernels, BYTE* a, const BYTE* b, int ap, int bp, int row, int h, int bits,
                       double weight, IScriptEnvironment* env) {
  const int bytes = bits == 8 ? 1 : bits == 32 ? 4 : 2;
  cp_plane_config c{};
  c.format = {bits == 8 ? CP_U8 : bits == 32 ? CP_F32 : CP_U16, bits};
  c.operation = CP_MIX;
  c.opacity = weight;
  const cp_const_plane source{a, ap, bytes};
  const int status = kernels->process_plane(&c, source, {b, bp, bytes}, nullptr, nullptr, nullptr, {a, ap, bytes},
                                            {row / bytes, h, 0, h});
  if (status != CP_OK)
    env->ThrowError("ConvertFPS: invalid blend kernel arguments (%d)", status);
}
} // namespace aif::filters::frame_rate
