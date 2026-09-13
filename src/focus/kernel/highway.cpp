// SPDX-License-Identifier: GPL-2.0-or-later
// With the inherited AviSynth linking exception; see LICENSE.
#include "backend.h"
#ifndef AIF_SCALAR_ONLY
#include "highway_config.h"
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "highway_inl.h"
#endif

#if defined(AIF_SCALAR_ONLY) || HWY_ONCE
extern "C" uint32_t aif_focus_supported_cpu(void) {
  static const uint32_t flags = [] {
    uint32_t result = 0;
#ifndef AIF_SCALAR_ONLY
    const int64_t supported = hwy::SupportedTargets();
#define AIF_TARGET(feature, target, choose)                                                                            \
  if (supported & target)                                                                                              \
    result |= feature;
#include "targets.inc"
#undef AIF_TARGET
#endif
    return result;
  }();
  return flags;
}
extern "C" uint32_t aif_focus_selected_cpu(uint32_t allowed) {
  allowed &= aif_focus_supported_cpu();
  if (!(allowed & AIF_FOCUS_NEON))
    allowed &= ~uint32_t(AIF_FOCUS_SVE2);
#ifndef AIF_SCALAR_ONLY
#define AIF_TARGET(feature, target, choose)                                                                            \
  if (allowed & feature)                                                                                               \
    return feature;
#include "targets.inc"
#undef AIF_TARGET
#endif
  return 0;
}
const aif::focus::Backend* aif::focus::backend(uint32_t allowed) {
  const uint32_t selected = aif_focus_selected_cpu(allowed);
#ifndef AIF_SCALAR_ONLY
  switch (selected) {
#define AIF_TARGET(feature, target, choose)                                                                            \
  case feature: {                                                                                                      \
    static const Backend table = {choose(Vertical), choose(Horizontal), choose(Temporal), choose(Sad)};                \
    return &table;                                                                                                     \
  }
#include "targets.inc"
#undef AIF_TARGET
    default:
      break;
  }
#else
  (void)selected;
#endif
  return nullptr;
}
#endif
