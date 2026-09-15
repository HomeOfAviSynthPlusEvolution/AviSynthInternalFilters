#include "chroma_scalar.h"
#ifndef AIF_SCALAR_ONLY
#include "highway_config.h"
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "chroma_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
#include "chroma_highway_inl.h"
#endif
#if defined(AIF_SCALAR_ONLY) || HWY_ONCE
namespace aif::filters::overlay {
Chroma select_chroma(int64_t allowed) {
#ifndef AIF_SCALAR_ONLY
  const int64_t targets = allowed & hwy::SupportedTargets();
#if HWY_TARGETS & HWY_AVX10_2
  if (targets & HWY_AVX10_2)
    return HWY_CHOOSE_AVX10_2(ChromaRowsDispatch);
#endif
#if HWY_TARGETS & HWY_AVX3_SPR
  if (targets & HWY_AVX3_SPR)
    return HWY_CHOOSE_AVX3_SPR(ChromaRowsDispatch);
#endif
#if HWY_TARGETS & HWY_AVX3_ZEN4
  if (targets & HWY_AVX3_ZEN4)
    return HWY_CHOOSE_AVX3_ZEN4(ChromaRowsDispatch);
#endif
#if HWY_TARGETS & HWY_AVX3_DL
  if (targets & HWY_AVX3_DL)
    return HWY_CHOOSE_AVX3_DL(ChromaRowsDispatch);
#endif
#if HWY_TARGETS & HWY_AVX3
  if (targets & HWY_AVX3)
    return HWY_CHOOSE_AVX3(ChromaRowsDispatch);
#endif
#if HWY_TARGETS & HWY_AVX2
  if (targets & HWY_AVX2)
    return HWY_CHOOSE_AVX2(ChromaRowsDispatch);
#endif
#if HWY_TARGETS & HWY_SSE4
  if (targets & HWY_SSE4)
    return HWY_CHOOSE_SSE4(ChromaRowsDispatch);
#endif
#if HWY_TARGETS & HWY_SSSE3
  if (targets & HWY_SSSE3)
    return HWY_CHOOSE_SSSE3(ChromaRowsDispatch);
#endif
#if HWY_TARGETS & HWY_SSE2
  if (targets & HWY_SSE2)
    return HWY_CHOOSE_SSE2(ChromaRowsDispatch);
#endif
#if HWY_TARGETS & HWY_NEON_WITHOUT_AES
  if (targets & HWY_NEON_WITHOUT_AES)
    return HWY_CHOOSE_NEON_WITHOUT_AES(ChromaRowsDispatch);
#endif
#else
  (void)allowed;
#endif
  return chroma_scalar;
}
} // namespace aif::filters::overlay
#endif
