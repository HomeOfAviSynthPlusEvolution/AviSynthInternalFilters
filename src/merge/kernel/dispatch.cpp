#include "merge/kernel.h"
#include <composite/dispatch.h>
#ifndef AIF_SCALAR_ONLY
#include <hwy/detect_targets.h>
#endif
#include <cmath>
#include <new>
extern "C" uint32_t aif_merge_supported_cpu(void) {
  uint32_t cpu = 0;
#ifndef AIF_SCALAR_ONLY
  const auto t = cp_supported_targets();
  if (t & HWY_SSE2)
    cpu |= AIF_MERGE_SSE2;
  if (t & HWY_SSSE3)
    cpu |= AIF_MERGE_SSSE3;
  if (t & HWY_NEON_WITHOUT_AES)
    cpu |= AIF_MERGE_NEON;
  if (t & HWY_SSE4)
    cpu |= AIF_MERGE_SSE4;
  if (t & HWY_AVX2)
    cpu |= AIF_MERGE_AVX2;
  if (t & HWY_AVX3)
    cpu |= AIF_MERGE_AVX3;
  if (t & HWY_AVX3_DL)
    cpu |= AIF_MERGE_AVX3_DL;
  if (t & HWY_AVX3_ZEN4)
    cpu |= AIF_MERGE_AVX3_ZEN4;
  if (t & HWY_AVX3_SPR)
    cpu |= AIF_MERGE_AVX3_SPR;
  if (t & HWY_AVX10_2)
    cpu |= AIF_MERGE_AVX10_2;
#endif
  return cpu;
}
struct aif_merge_plan {
  const cp_kernels* primary = nullptr;
  const cp_kernels* large_u16_half = nullptr;
};
namespace {
aif_merge_plan select_plan(uint32_t cpu) {
  int64_t mask = 0;
#ifndef AIF_SCALAR_ONLY
  if (cpu & 1)
    mask |= HWY_SSE2;
  if (cpu & 2)
    mask |= HWY_SSSE3;
  if (cpu & 4)
    mask |= HWY_NEON_WITHOUT_AES;
  if (cpu & 8)
    mask |= HWY_SSE4;
  if (cpu & 16)
    mask |= HWY_AVX2;
  if (cpu & AIF_MERGE_AVX3)
    mask |= HWY_AVX3;
  if (cpu & AIF_MERGE_AVX3_DL)
    mask |= HWY_AVX3_DL;
  if (cpu & AIF_MERGE_AVX3_ZEN4)
    mask |= HWY_AVX3_ZEN4;
  if (cpu & AIF_MERGE_AVX3_SPR)
    mask |= HWY_AVX3_SPR;
  if (cpu & AIF_MERGE_AVX10_2)
    mask |= HWY_AVX10_2;
#else
  (void)cpu;
#endif
  const auto target = cp_choose_target(mask);
  aif_merge_plan plan{cp_get_kernels(target), nullptr};
#ifndef AIF_SCALAR_ONLY
  // AVX2 wins for U16 half-weight from the measured 540p size onward.
  // Resolve once; SPR retains its full-width path.
  if ((target == HWY_AVX3 || target == HWY_AVX3_DL || target == HWY_AVX3_ZEN4) && (cp_supported_targets() & HWY_AVX2))
    plan.large_u16_half = cp_get_kernels(HWY_AVX2);
#endif
  return plan;
}
int validate(int bp, int sp, int w, int h, int bits, int step, double weight) {
  if (w == 0 || h == 0)
    return CP_OK;
  if ((bits != 8 && bits != 10 && bits != 12 && bits != 14 && bits != 16 && bits != 32) || bp <= 0 || sp <= 0 ||
      !std::isfinite(weight) || weight < 0 || weight > 1)
    return CP_INVALID_ARGUMENT;
  int bytes = bits == 8 ? 1 : bits == 32 ? 4 : 2;
  if (step != bytes && !(bits == 8 && step == 2))
    return CP_INVALID_ARGUMENT;
  return CP_OK;
}
int apply(const aif_merge_plan& plan, uint8_t* base, const uint8_t* source, int bp, int sp, int w, int h, int bits,
          int step, double weight) {
  const auto* fn = plan.primary;
  if (plan.large_u16_half && bits == 16 && weight == .5 && uint64_t(w > 0 ? w : 0) * (h > 0 ? h : 0) >= 960u * 540u)
    fn = plan.large_u16_half;
  cp_plane_config config{};
  config.format = {bits == 8 ? CP_U8 : bits == 32 ? CP_F32 : CP_U16, bits};
  config.operation = CP_MIX;
  config.opacity = weight;
  return fn->process_plane(&config, {base, bp, step}, {source, sp, step}, nullptr, nullptr, nullptr, {base, bp, step},
                           {w, h, 0, h});
}
} // namespace
extern "C" int aif_merge_create(uint32_t cpu, aif_merge_plan** out) {
  if (!out)
    return CP_INVALID_ARGUMENT;
  *out = new (std::nothrow) aif_merge_plan(select_plan(cpu));
  return *out ? CP_OK : CP_INVALID_ARGUMENT;
}
extern "C" void aif_merge_destroy(aif_merge_plan* plan) {
  delete plan;
}
extern "C" int aif_merge_mix_with_plan(const aif_merge_plan* plan, uint8_t* base, const uint8_t* source, int bp, int sp,
                                       int w, int h, int bits, int step, double weight) {
  if (!plan)
    return CP_INVALID_ARGUMENT;
  const int status = validate(bp, sp, w, h, bits, step, weight);
  if (status || w == 0 || h == 0)
    return status;
  return apply(*plan, base, source, bp, sp, w, h, bits, step, weight);
}
