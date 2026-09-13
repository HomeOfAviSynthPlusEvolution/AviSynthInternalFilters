#pragma once
#include <avisynth.h>
namespace aif::filters::channel_display {
class ShowChannel : public GenericVideoFilter
/**
    * Class to set the RGB components to the alpha mask
  **/
{
public:
  ShowChannel(PClip _child, const char* _pixel_type, int _channel, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    (void)frame_range;
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl Create(AVSValue args, void* channel, IScriptEnvironment* env);

private:
  int channel;
  const int input_type;
  const int pixelsize;
  const int bits_per_pixel;
  bool input_type_is_planar_rgb;
  bool input_type_is_planar_rgba;
  bool input_type_is_yuv;
  bool input_type_is_yuva;
  bool input_type_is_planar;
  bool input_type_is_packed_rgb;
  bool target_hasalpha;
  bool source_hasalpha;
};
} // namespace aif::filters::channel_display
