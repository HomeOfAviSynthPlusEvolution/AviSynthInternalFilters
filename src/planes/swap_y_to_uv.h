// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include <cstdint>
namespace aif::filters::planes {
class SwapYToUV : public GenericVideoFilter
/**
  * SwapYToYUVs planar channels
 **/
{
  const uint32_t cpu_mask_;

public:
  SwapYToUV(PClip _child, PClip _clip, PClip _clipY, PClip _clipA, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    (void)frame_range;
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl CreateYToUV(AVSValue args, void* user_data, IScriptEnvironment* env);
  static AVSValue __cdecl CreateYToYUV(AVSValue args, void* user_data, IScriptEnvironment* env);
  static AVSValue __cdecl CreateYToYUVA(AVSValue args, void* user_data, IScriptEnvironment* env);

private:
  PClip clip, clipY, clipA;
};
} // namespace aif::filters::planes
