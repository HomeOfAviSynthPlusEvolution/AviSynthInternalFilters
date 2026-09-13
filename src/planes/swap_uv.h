// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
namespace aif::filters::planes {
class SwapUV : public GenericVideoFilter
/**
  * SwapUVs planar channels
 **/
{
public:
  SwapUV(PClip _child, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    (void)frame_range;
    if (cachehints == CACHE_GET_DEV_TYPE) {
      return (vi.IsPlanar() && (child->GetVersion() >= 5)) ? child->SetCacheHints(CACHE_GET_DEV_TYPE, 0) : 0;
    }
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl CreateSwapUV(AVSValue args, void* user_data, IScriptEnvironment* env);
};
} // namespace aif::filters::planes
