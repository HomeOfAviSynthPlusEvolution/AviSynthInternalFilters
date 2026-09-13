#include "merge/kernel.h"
#include <composite/dispatch.h>
#ifndef AIF_SCALAR_ONLY
#include <hwy/detect_targets.h>
#endif
#include <cmath>
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
extern "C" int aif_merge_mix(uint8_t* base, const uint8_t* source, int bp, int sp, int w, int h, int bits, int step,
                             double weight, uint32_t cpu) {
  if (w == 0 || h == 0)
    return CP_OK;
  if ((bits != 8 && bits != 10 && bits != 12 && bits != 14 && bits != 16 && bits != 32) || bp <= 0 || sp <= 0 ||
      !std::isfinite(weight) || weight < 0 || weight > 1)
    return CP_INVALID_ARGUMENT;
  int bytes = bits == 8 ? 1 : bits == 32 ? 4 : 2;
  if (step != bytes && !(bits == 8 && step == 2))
    return CP_INVALID_ARGUMENT;
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
  auto target = cp_choose_target(mask);
#ifndef AIF_SCALAR_ONLY
  // Measured large U16 averages favor AVX2; keep unmeasured targets untouched.
  if (bits == 16 && weight == .5 && uint64_t(w > 0 ? w : 0) * (h > 0 ? h : 0) >= 1920u * 1080u &&
      (target == HWY_AVX3 || target == HWY_AVX3_DL || target == HWY_AVX3_ZEN4) && (cp_supported_targets() & HWY_AVX2))
    target = HWY_AVX2;
#endif
  auto fn = cp_get_kernels(target);
  cp_plane_config config{};
  config.format = {bits == 8 ? CP_U8 : bits == 32 ? CP_F32 : CP_U16, bits};
  config.operation = CP_MIX;
  config.opacity = weight;
  return fn->process_plane(&config, {base, bp, step}, {source, sp, step}, nullptr, nullptr, nullptr, {base, bp, step},
                           {w, h, 0, h});
}
