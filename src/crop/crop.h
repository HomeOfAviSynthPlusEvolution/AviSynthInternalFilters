// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
namespace aif::filters::crop {
class Crop : public GenericVideoFilter
/**
  * Class to crop a video
 **/
{
public:
  Crop(int _left, int _top, int _width, int _height, bool _align, PClip _child, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override;

  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);

private:
  /*const*/ int left_bytes, top, align;
  int xsub, ysub;
  bool isRGBPfamily;
  bool hasAlpha;
};
PClip make_crop(PClip clip, int left, int top, int width, int height, bool align, IScriptEnvironment* env);
AVSValue __cdecl create_crop_bottom(AVSValue, void*, IScriptEnvironment*);
} // namespace aif::filters::crop
