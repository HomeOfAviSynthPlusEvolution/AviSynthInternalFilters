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

// Avisynth filter: YUV merge / Swap planes
// by Klaus Post (kp@interact.dk)
// adapted by Richard Berg (avisynth-dev@richardberg.net)
// iSSE code by Ian Brabham

#include "merge_luma.h"
#include "kernel_adapter.h"
#include <cmath>
namespace aif::filters::merge {
MergeLuma::MergeLuma(PClip _child, PClip _clip, float _weight, IScriptEnvironment* env)
    : GenericVideoFilter(_child), clip(_clip), weight(_weight) {
  if (std::isnan(_weight))
    env->ThrowError("MergeLuma: weight cannot be NaN");

  const VideoInfo& vi2 = clip->GetVideoInfo();

  if (!(vi.IsYUV() || vi.IsYUVA()) || !(vi2.IsYUV() || vi2.IsYUVA()))
    env->ThrowError("MergeLuma: YUV data only (no RGB); use ConvertToYUY2, ConvertToYV12/16/24 or ConvertToYUVxxx");

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();

  if (!vi.IsSameColorspace(vi2)) { // Since this is luma we allow all planar formats to be merged.
    if (!(vi.IsPlanar() && vi2.IsPlanar())) {
      env->ThrowError("MergeLuma: YUV data is not same type. YUY2 and planar images doesn't mix.");
    }
    if (pixelsize != vi2.ComponentSize()) {
      env->ThrowError("MergeLuma: YUV data bit depth is not same.");
    }
  }

  if (vi.width != vi2.width || vi.height != vi2.height)
    env->ThrowError("MergeLuma: Images must have same width and height!");

  if (weight < 0.0f)
    weight = 0.0f;
  if (weight > 1.0f)
    weight = 1.0f;
}

PVideoFrame __stdcall MergeLuma::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);

  if (weight < 0.0039f)
    return src;

  PVideoFrame luma = clip->GetFrame(n, env);

  if (vi.IsYUY2()) {
    env->MakeWritable(&src);
    mix({src->GetWritePtr(), src->GetPitch(), 2}, {luma->GetReadPtr(), luma->GetPitch(), 2},
        {vi.width, vi.height, 0, vi.height}, 8, weight < 0.9961f ? weight : 1.0, env);
    return src;
  } // Planar
  if (weight > 0.9961f) {
    // 2nd clip weight is almost 100%: no merge, just copy
    const VideoInfo& vi2 = clip->GetVideoInfo();
    if (luma->IsWritable() && vi.IsSameColorspace(vi2)) {
      if (luma->GetRowSize(PLANAR_U)) {
        luma->GetWritePtr(PLANAR_Y); //Must be requested BUT only if we actually do something
        env->BitBlt(luma->GetWritePtr(PLANAR_U), luma->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_U),
                    src->GetPitch(PLANAR_U), src->GetRowSize(PLANAR_U), src->GetHeight(PLANAR_U));
        env->BitBlt(luma->GetWritePtr(PLANAR_V), luma->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_V),
                    src->GetPitch(PLANAR_V), src->GetRowSize(PLANAR_V), src->GetHeight(PLANAR_V));
      }
      if (luma->GetPitch(PLANAR_A)) // copy Alpha if exists
        env->BitBlt(luma->GetWritePtr(PLANAR_A), luma->GetPitch(PLANAR_A), src->GetReadPtr(PLANAR_A),
                    src->GetPitch(PLANAR_A), src->GetRowSize(PLANAR_A), src->GetHeight(PLANAR_A));

      return luma;
    } else { // avoid the cost of 2 chroma blits
      PVideoFrame dst = env->NewVideoFrameP(vi, &luma);

      env->BitBlt(dst->GetWritePtr(PLANAR_Y), dst->GetPitch(PLANAR_Y), luma->GetReadPtr(PLANAR_Y),
                  luma->GetPitch(PLANAR_Y), luma->GetRowSize(PLANAR_Y), luma->GetHeight(PLANAR_Y));
      if (src->GetRowSize(PLANAR_U) && dst->GetRowSize(PLANAR_U)) {
        env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_U),
                    src->GetPitch(PLANAR_U), src->GetRowSize(PLANAR_U), src->GetHeight(PLANAR_U));
        env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_V),
                    src->GetPitch(PLANAR_V), src->GetRowSize(PLANAR_V), src->GetHeight(PLANAR_V));
      }
      if (dst->GetPitch(PLANAR_A) && src->GetPitch(PLANAR_A)) // copy Alpha if in both clip exists
        env->BitBlt(dst->GetWritePtr(PLANAR_A), dst->GetPitch(PLANAR_A), src->GetReadPtr(PLANAR_A),
                    src->GetPitch(PLANAR_A), src->GetRowSize(PLANAR_A), src->GetHeight(PLANAR_A));

      return dst;
    }
  } else { // weight <= 0.9961f
    env->MakeWritable(&src);
    BYTE* srcpY = (BYTE*)src->GetWritePtr(PLANAR_Y);
    BYTE* lumapY = (BYTE*)luma->GetReadPtr(PLANAR_Y);
    int src_pitch = src->GetPitch(PLANAR_Y);
    int luma_pitch = luma->GetPitch(PLANAR_Y);
    int src_rowsize = src->GetRowSize(PLANAR_Y);
    int src_height = src->GetHeight(PLANAR_Y);

    merge_plane(srcpY, lumapY, src_pitch, luma_pitch, src_rowsize, src_height, weight, pixelsize, bits_per_pixel, env);
  }

  return src;
}

AVSValue __cdecl MergeLuma::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new MergeLuma(args[0].AsClip(), args[1].AsClip(), (float)args[2].AsFloat(1.0f), env);
}
} // namespace aif::filters::merge
