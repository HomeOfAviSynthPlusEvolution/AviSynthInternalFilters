#pragma once
#include <avisynth.h>
#include <cstdint>
namespace aif::filters::rgb_merge {
class MergeRGB : public GenericVideoFilter
/**
    * Class to load the RGB components from specified clips
  **/
{
  const uint32_t cpu_mask_;

public:
  MergeRGB(PClip _child, PClip _blue, PClip _green, PClip _red, PClip _alpha, const char* _pixel_type,
           IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    (void)frame_range;
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl Create(AVSValue args, void* mode, IScriptEnvironment* env);

private:
  const PClip blue, green, red, alpha;
  const VideoInfo &viB, &viG, &viR, &viA;
  const char* myname;
};
} // namespace aif::filters::rgb_merge
