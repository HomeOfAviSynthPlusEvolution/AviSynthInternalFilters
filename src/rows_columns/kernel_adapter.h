#pragma once
#include <avisynth.h>
#include "rows_columns/kernel.h"
#include <vector>
#include <algorithm>
namespace aif::filters::rows_columns {
inline constexpr uint32_t allowed_cpu_flags(uint64_t flags) {
#if defined(ARM64) || defined(ARM32)
  return flags & CPUF_ARM_NEON ? AIF_ROWS_COLUMNS_NEON : 0;
#else
  uint32_t cpu = (flags & CPUF_SSE2 ? AIF_ROWS_COLUMNS_SSE2 : 0) | (flags & CPUF_SSSE3 ? AIF_ROWS_COLUMNS_SSSE3 : 0);
  const auto sse4 = CPUF_SSE4_1 | CPUF_SSE4_2 | CPUF_AES;
  if ((flags & sse4) != sse4)
    return cpu;
  cpu |= AIF_ROWS_COLUMNS_SSE4;
  const auto avx2 = CPUF_AVX | CPUF_AVX2 | CPUF_FMA3 | CPUF_F16C;
  if ((flags & avx2) != avx2)
    return cpu;
  cpu |= AIF_ROWS_COLUMNS_AVX2;
  const auto avx3 = CPUF_AVX512F | CPUF_AVX512CD | CPUF_AVX512BW | CPUF_AVX512DQ | CPUF_AVX512VL;
  if ((flags & avx3) != avx3)
    return cpu;
  cpu |= AIF_ROWS_COLUMNS_AVX3;
  const auto dl = CPUF_AVX512VNNI | CPUF_AVX512VBMI | CPUF_AVX512VBMI2 | CPUF_AVX512BITALG | CPUF_AVX512VPOPCNTDQ;
  if ((flags & dl) != dl)
    return cpu;
  cpu |= AIF_ROWS_COLUMNS_AVX3_DL;
  if (flags & CPUF_AVX512BF16) {
    cpu |= AIF_ROWS_COLUMNS_AVX3_ZEN4;
    if (flags & CPUF_AVX512FP16)
      cpu |= AIF_ROWS_COLUMNS_AVX3_SPR;
  }
  return cpu;
#endif
}
inline void column_frame(const std::vector<PVideoFrame>& frames, PVideoFrame& dst, const VideoInfo& vi, int period,
                         int phase, bool weave, IScriptEnvironment* env) {
  const int yuv[] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A}, rgb[] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
  const int* planes = vi.IsRGB() ? rgb : yuv;
  const uint32_t cpu = allowed_cpu_flags(env->GetCPUFlagsEx());
  for (int p = 0; p < (vi.IsPlanar() ? vi.NumComponents() : 1); ++p) {
    int plane = vi.IsPlanar() ? planes[p] : 0;
    int size = vi.IsPlanar() ? vi.ComponentSize() : vi.IsYUY2() ? 0 : vi.ComponentSize() * vi.NumComponents();
    std::vector<const uint8_t*> src;
    std::vector<int> sp;
    for (auto& f : frames) {
      src.push_back(f->GetReadPtr(plane));
      sp.push_back(f->GetPitch(plane));
    }
    int count = (weave ? frames[0]->GetRowSize(plane) : dst->GetRowSize(plane)) / (size ? size : 2);
    if (aif_rows_columns_process(src.data(), sp.data(), dst->GetWritePtr(plane), dst->GetPitch(plane), count,
                                 dst->GetHeight(plane), size, period, phase, weave, cpu))
      env->ThrowError("Columns: kernel failed");
  }
}
} // namespace aif::filters::rows_columns
