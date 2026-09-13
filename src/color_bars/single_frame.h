// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
namespace aif::filters::color_bars {
class SingleFrame final : public GenericVideoFilter {
public:
  explicit SingleFrame(PClip child);

  PVideoFrame __stdcall GetFrame(const int, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(const int cachehints, const int) override;
};

} // namespace aif::filters::color_bars
