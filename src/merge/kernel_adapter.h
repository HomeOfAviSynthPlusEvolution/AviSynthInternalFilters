#pragma once
#include <avisynth.h>
#include "../common/host_cpu.h"
#include <memory>
#include "merge/kernel.h"
namespace aif::filters::merge {
inline constexpr uint32_t allowed_cpu_flags(uint64_t flags) {
#if defined(ARM64) || defined(ARM32)
  return flags & CPUF_ARM_NEON ? AIF_MERGE_NEON : 0;
#else
  uint32_t cpu = (flags & CPUF_SSE2 ? AIF_MERGE_SSE2 : 0) | (flags & CPUF_SSSE3 ? AIF_MERGE_SSSE3 : 0);
  const auto sse4 = CPUF_SSE4_1 | CPUF_SSE4_2 | CPUF_AES;
  if ((flags & sse4) != sse4)
    return cpu;
  cpu |= AIF_MERGE_SSE4;
  const auto avx2 = CPUF_AVX | CPUF_AVX2 | CPUF_FMA3 | CPUF_F16C;
  if ((flags & avx2) != avx2)
    return cpu;
  cpu |= AIF_MERGE_AVX2;
  const auto avx3 = CPUF_AVX512F | CPUF_AVX512CD | CPUF_AVX512BW | CPUF_AVX512DQ | CPUF_AVX512VL;
  if ((flags & avx3) != avx3)
    return cpu;
  cpu |= AIF_MERGE_AVX3;
  const auto dl = CPUF_AVX512VNNI | CPUF_AVX512VBMI | CPUF_AVX512VBMI2 | CPUF_AVX512BITALG | CPUF_AVX512VPOPCNTDQ;
  if ((flags & dl) != dl)
    return cpu;
  cpu |= AIF_MERGE_AVX3_DL;
  if (flags & CPUF_AVX512BF16) {
    cpu |= AIF_MERGE_AVX3_ZEN4;
    if (flags & CPUF_AVX512FP16)
      cpu |= AIF_MERGE_AVX3_SPR;
  }
  return cpu;
#endif
}
inline uint32_t allowed_cpu(IScriptEnvironment* env) {
  return allowed_cpu_flags(aif::filters::host_cpu_flags(env));
}
using MergePlan = std::unique_ptr<aif_merge_plan, decltype(&aif_merge_destroy)>;
inline MergePlan make_plan(IScriptEnvironment* env) {
  aif_merge_plan* plan = nullptr;
  if (aif_merge_create(allowed_cpu(env), &plan))
    env->ThrowError("Merge: cannot create kernel plan");
  return MergePlan(plan, aif_merge_destroy);
}
struct WritePlane {
  uint8_t* data;
  int pitch, step;
};
struct ReadPlane {
  const uint8_t* data;
  int pitch, step;
};
struct Rows {
  int width, height, first, count;
};
inline void mix(WritePlane base, ReadPlane source, Rows rows, int bits, double weight, const aif_merge_plan* plan,
                IScriptEnvironment* env) {
  if (aif_merge_mix_with_plan(plan, base.data, source.data, base.pitch, source.pitch, rows.width, rows.height, bits,
                              base.step, weight))
    env->ThrowError("Merge: kernel arguments invalid");
}
inline void merge_plane(uint8_t* base, const uint8_t* source, int bp, int sp, int row, int h, float weight, int bytes,
                        int bits, const aif_merge_plan* plan, IScriptEnvironment* env) {
  if (weight > 0.4961f && weight < 0.5039f)
    weight = 0.5f;
  mix({base, bp, bytes}, {source, sp, bytes}, {row / bytes, h, 0, h}, bits, weight, plan, env);
}
} // namespace aif::filters::merge
