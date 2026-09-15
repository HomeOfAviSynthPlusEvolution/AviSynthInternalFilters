// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include <cstdint>
namespace aif::filters::planes {
class SwapUVToY : public GenericVideoFilter
/**
  * SwapUVToYs planar channels
 **/
{
  const uint32_t cpu_mask_;

public:
  SwapUVToY(PClip _child, int _mode, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    (void)frame_range;
    if (cachehints == CACHE_GET_DEV_TYPE) {
      return (!vi.IsYUY2() && (child->GetVersion() >= 5)) ? child->SetCacheHints(CACHE_GET_DEV_TYPE, 0) : 0;
    }
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl CreateUToY(AVSValue args, void* user_data, IScriptEnvironment* env);
  static AVSValue __cdecl CreateVToY(AVSValue args, void* user_data, IScriptEnvironment* env);
  static AVSValue __cdecl CreateYToY8(AVSValue args, void* user_data, IScriptEnvironment* env);
  static AVSValue __cdecl CreateUToY8(AVSValue args, void* user_data, IScriptEnvironment* env);
  static AVSValue __cdecl CreateVToY8(AVSValue args, void* user_data, IScriptEnvironment* env);
  static AVSValue __cdecl CreateAnyToY8(AVSValue args, void* user_data, IScriptEnvironment* env);
  static AVSValue __cdecl CreatePlaneToY8(AVSValue args, void* user_data, IScriptEnvironment* env);

  enum { UToY = 1, VToY, UToY8, VToY8, YUY2UToY8, YUY2VToY8, AToY8, RToY8, GToY8, BToY8, YToY8 };

private:
  int mode;
};
} // namespace aif::filters::planes
