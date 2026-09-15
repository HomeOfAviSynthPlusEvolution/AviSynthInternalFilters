// SPDX-License-Identifier: GPL-2.0-or-later
#include <avisynth.h>
#include "mask/filters.h"

const AVS_Linkage* AVS_linkage = nullptr;
#if defined(_WIN32)
#define AIF_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define AIF_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

AIF_PLUGIN_EXPORT const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* environment,
                                                            const AVS_Linkage* const linkage) {
  environment->CheckVersion(8); // Frame properties, allocation and alpha subframes.
  AVS_linkage = linkage;
  aif::filters::mask::register_filters(environment);
  return "AviSynthInternalFilters: Mask migration baseline";
}
