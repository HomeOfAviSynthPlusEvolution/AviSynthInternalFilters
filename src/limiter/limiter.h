// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include "limiter/kernel.h"
namespace aif::filters::limiter {
class Limiter final : public GenericVideoFilter {
  aif_limiter_limits limits_{};
  int show_;
  uint32_t cpu_ = 0;

public:
  Limiter(PClip, float, float, float, float, int, bool, IScriptEnvironment*);
  PVideoFrame __stdcall GetFrame(int, IScriptEnvironment*) override;
  int __stdcall SetCacheHints(int hint, int) override { return hint == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0; }
  static AVSValue __cdecl Create(AVSValue, void*, IScriptEnvironment*);
};
} // namespace aif::filters::limiter
