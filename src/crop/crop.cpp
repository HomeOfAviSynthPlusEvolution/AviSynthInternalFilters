// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://avisynth.nl

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

#include "crop.h"
namespace aif::filters::crop {
Crop::Crop(int _left, int _top, int _width, int _height, bool _align, PClip _child, IScriptEnvironment* env)
    : GenericVideoFilter(_child), align(FRAME_ALIGN - 1), xsub(0), ysub(0) {
  (void)_align;
  // _align parameter exists only for the backward compatibility.

  /* Negative values -> VDub-style syntax
     Namely, Crop(a, b, -c, -d) will crop c pixels from the right and d pixels from the bottom.
     Flags on 0 values too since AFAICT it's much more useful to this syntax than the standard one. */
  if ((_left < 0) || (_top < 0))
    env->ThrowError("Crop: Top and Left must be more than 0");

  if (_width <= 0)
    _width = vi.width - _left + _width;
  if (_height <= 0)
    _height = vi.height - _top + _height;

  if (_width <= 0)
    env->ThrowError("Crop: Destination width is 0 or less.");

  if (_height <= 0)
    env->ThrowError("Crop: Destination height is 0 or less.");

  if (_left + _width > vi.width || _top + _height > vi.height)
    env->ThrowError("Crop: you cannot use crop to enlarge or 'shift' a clip");

  isRGBPfamily = vi.IsPlanarRGB() || vi.IsPlanarRGBA();
  hasAlpha = vi.IsPlanarRGBA() || vi.IsYUVA();

  if (vi.IsYUV() || vi.IsYUVA()) {
    if (vi.NumComponents() > 1) {
      xsub = vi.GetPlaneWidthSubsampling(PLANAR_U);
      ysub = vi.GetPlaneHeightSubsampling(PLANAR_U);
    }
    const int xmask = (1 << xsub) - 1;
    const int ymask = (1 << ysub) - 1;

    // YUY2, etc, ... can only crop to even pixel boundaries horizontally
    if (_left & xmask)
      env->ThrowError("Crop: YUV image can only be cropped by Mod %d (left side).", xmask + 1);
    if (_width & xmask)
      env->ThrowError("Crop: YUV image can only be cropped by Mod %d (right side).", xmask + 1);
    if (_top & ymask)
      env->ThrowError("Crop: YUV image can only be cropped by Mod %d (top).", ymask + 1);
    if (_height & ymask)
      env->ThrowError("Crop: YUV image can only be cropped by Mod %d (bottom).", ymask + 1);
  } else if (!isRGBPfamily) {
    // RGB is upside-down
    _top = vi.height - _height - _top;
  }

  left_bytes = vi.BytesFromPixels(_left);
  top = _top;
  vi.width = _width;
  vi.height = _height;
}

PVideoFrame Crop::GetFrame(int n, IScriptEnvironment* env) {

  PVideoFrame frame = child->GetFrame(n, env);

  int plane0 = isRGBPfamily ? PLANAR_G : PLANAR_Y;
  int plane1 = isRGBPfamily ? PLANAR_B : PLANAR_U;
  int plane2 = isRGBPfamily ? PLANAR_R : PLANAR_V;

  const BYTE* srcp0 = frame->GetReadPtr(plane0) + top * frame->GetPitch(plane0) + left_bytes;
  const BYTE* srcp1 = frame->GetPitch(plane1)
                          ? frame->GetReadPtr(plane1) + (top >> ysub) * frame->GetPitch(plane1) + (left_bytes >> xsub)
                          : nullptr;
  const BYTE* srcp2 = frame->GetPitch(plane2)
                          ? frame->GetReadPtr(plane2) + (top >> ysub) * frame->GetPitch(plane2) + (left_bytes >> xsub)
                          : nullptr;

  size_t _align;

  if (frame->GetPitch(plane1) &&
      (!vi.IsYV12() || env->PlanarChromaAlignment(IScriptEnvironment::PlanarChromaAlignmentTest)))
    _align = this->align & ((size_t)srcp0 | (size_t)srcp1 | (size_t)srcp2);
  else
    _align = this->align & (size_t)srcp0;

  if (0 != _align) {
    PVideoFrame dst = env->NewVideoFrameP(vi, &frame, (int)align + 1);

    env->BitBlt(dst->GetWritePtr(plane0), dst->GetPitch(plane0), srcp0, frame->GetPitch(plane0),
                dst->GetRowSize(plane0), dst->GetHeight(plane0));

    if (frame->GetPitch(plane1))
      env->BitBlt(dst->GetWritePtr(plane1), dst->GetPitch(plane1), srcp1, frame->GetPitch(plane1),
                  dst->GetRowSize(plane1), dst->GetHeight(plane1));

    if (frame->GetPitch(plane2))
      env->BitBlt(dst->GetWritePtr(plane2), dst->GetPitch(plane2), srcp2, frame->GetPitch(plane2),
                  dst->GetRowSize(plane2), dst->GetHeight(plane2));

    if (hasAlpha)
      env->BitBlt(dst->GetWritePtr(PLANAR_A), dst->GetPitch(PLANAR_A),
                  frame->GetReadPtr(PLANAR_A) + top * frame->GetPitch(PLANAR_A) + left_bytes, frame->GetPitch(PLANAR_A),
                  dst->GetRowSize(PLANAR_A), dst->GetHeight(PLANAR_A));

    return dst;
  }

  // subframe is preserving frame properties
  if (!frame->GetPitch(plane1))
    return env->Subframe(frame, top * frame->GetPitch() + left_bytes, frame->GetPitch(), vi.RowSize(), vi.height);
  else {
    if (hasAlpha) {

      return env->SubframePlanarA(frame, top * frame->GetPitch() + left_bytes, frame->GetPitch(), vi.RowSize(),
                                  vi.height, (top >> ysub) * frame->GetPitch(plane1) + (left_bytes >> xsub),
                                  (top >> ysub) * frame->GetPitch(plane2) + (left_bytes >> xsub),
                                  frame->GetPitch(plane1), top * frame->GetPitch(PLANAR_A) + left_bytes);
    } else {
      return env->SubframePlanar(frame, top * frame->GetPitch() + left_bytes, frame->GetPitch(), vi.RowSize(),
                                 vi.height, (top >> ysub) * frame->GetPitch(plane1) + (left_bytes >> xsub),
                                 (top >> ysub) * frame->GetPitch(plane2) + (left_bytes >> xsub),
                                 frame->GetPitch(plane1));
    }
  }
}

int __stdcall Crop::SetCacheHints(int cachehints, int frame_range) {
  (void)frame_range;
  switch (cachehints) {
    case CACHE_GET_MTMODE:
      return MT_NICE_FILTER;
  }
  return 0;
}

AVSValue __cdecl Crop::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new Crop(args[1].AsInt(), args[2].AsInt(), args[3].AsInt(), args[4].AsInt(), args[5].AsBool(true),
                  args[0].AsClip(), env);
}

AVSValue __cdecl create_crop_bottom(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  const VideoInfo& vi = clip->GetVideoInfo();
  return new Crop(0, 0, vi.width, vi.height - args[1].AsInt(), 0, clip, env);
}
PClip make_crop(PClip clip, int left, int top, int width, int height, bool align, IScriptEnvironment* env) {
  return new Crop(left, top, width, height, align, clip, env);
}
} // namespace aif::filters::crop
