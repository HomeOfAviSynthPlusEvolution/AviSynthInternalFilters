// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include <cstdint>
namespace aif::filters::invert {
class Invert final : public GenericVideoFilter {
  bool selected_[7]{};
  uint32_t cpu_ = 0;

public:
  Invert(PClip, const char*, IScriptEnvironment*);
  PVideoFrame __stdcall GetFrame(int, IScriptEnvironment*) override;
  int __stdcall SetCacheHints(int hint, int) override { return hint == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0; }
  static AVSValue __cdecl Create(AVSValue, void*, IScriptEnvironment*);
};
} // namespace aif::filters::invert
