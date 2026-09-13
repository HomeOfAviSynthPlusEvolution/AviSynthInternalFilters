// SPDX-License-Identifier: GPL-2.0-or-later
#include <avisynth.h>
#include "focus/filters.h"

const AVS_Linkage* AVS_linkage = nullptr;
#if defined(_WIN32)
#define AIF_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define AIF_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

AIF_PLUGIN_EXPORT const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* environment,
                                                            const AVS_Linkage* const linkage) {
  AVS_linkage = linkage;
  aif::filters::focus::register_filters(environment);
  return "AviSynthInternalFilters: Focus migration baseline";
}
