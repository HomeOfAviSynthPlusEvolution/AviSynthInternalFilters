// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include <memory>
#include "greyscale/kernel.h"
namespace aif::filters::greyscale {
class Greyscale final : public GenericVideoFilter {
  std::unique_ptr<aif_greyscale_plan, decltype(&aif_greyscale_destroy)> plan_{nullptr, aif_greyscale_destroy};
  uint32_t cpu_;
  int out_range_ = 0;

public:
  Greyscale(PClip, const char*, IScriptEnvironment*);
  PVideoFrame __stdcall GetFrame(int, IScriptEnvironment*) override;
  int __stdcall SetCacheHints(int hint, int) override { return hint == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0; }
  static AVSValue __cdecl Create(AVSValue, void*, IScriptEnvironment*);
};
} // namespace aif::filters::greyscale
