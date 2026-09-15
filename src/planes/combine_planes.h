// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
namespace aif::filters::planes {
class CombinePlanes : public GenericVideoFilter
/**
  * SwapYToYUVs planar channels
  **/
{
public:
  CombinePlanes(PClip _child, PClip _clip2, PClip _clip3, PClip _clip4, PClip _sample, const char* _target_planes_str,
                const char* _source_planes_str, const char* _pixel_type, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    (void)frame_range;
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl CreateCombinePlanes(AVSValue args, void* user_data, IScriptEnvironment* env);

private:
  bool has_frame_pixel_type_;
  PClip clips[4];
  int pixelsize;
  int bits_per_pixel;
  int planecount;
  int source_planes[4];
  int target_planes[4];
};
} // namespace aif::filters::planes
