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

#include "merge_chroma.h"
#include "kernel_adapter.h"
#include <cmath>
namespace aif::filters::merge {
MergeChroma::MergeChroma(PClip _child, PClip _clip, float _weight, IScriptEnvironment* env)
    : GenericVideoFilter(_child), clip(_clip), weight(_weight) {
  if (std::isnan(_weight))
    env->ThrowError("MergeChroma: weight cannot be NaN");

  const VideoInfo& vi2 = clip->GetVideoInfo();

  if (!(vi.IsYUV() || vi.IsYUVA()) || !(vi2.IsYUV() || vi2.IsYUVA()))
    env->ThrowError("MergeChroma: YUV data only (no RGB); use ConvertToYUY2, ConvertToYV12/16/24 or ConvertToYUVxxx");

  if (!(vi.IsSameColorspace(vi2)))
    env->ThrowError("MergeChroma: YUV images must have same data type.");

  if (vi.width != vi2.width || vi.height != vi2.height)
    env->ThrowError("MergeChroma: Images must have same width and height!");

  if (weight < 0.0f)
    weight = 0.0f;
  if (weight > 1.0f)
    weight = 1.0f;

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();
  plan_ = make_plan(env);
}

PVideoFrame __stdcall MergeChroma::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);

  if (weight < 0.0039f)
    return src;

  PVideoFrame chroma = clip->GetFrame(n, env);

  int h = src->GetHeight();
  int w = src->GetRowSize(); // active row bytes

  if (weight < 0.9961f) {
    if (vi.IsYUY2()) {
      env->MakeWritable(&src);
      mix({src->GetWritePtr() + 1, src->GetPitch(), 2}, {chroma->GetReadPtr() + 1, chroma->GetPitch(), 2},
          {w / 2, h, 0, h}, 8, weight, plan_.get(), env);
    } else { // Planar YUV
      env->MakeWritable(&src);
      src->GetWritePtr(PLANAR_Y); //Must be requested

      BYTE* srcpU = (BYTE*)src->GetWritePtr(PLANAR_U);
      BYTE* chromapU = (BYTE*)chroma->GetReadPtr(PLANAR_U);
      BYTE* srcpV = (BYTE*)src->GetWritePtr(PLANAR_V);
      BYTE* chromapV = (BYTE*)chroma->GetReadPtr(PLANAR_V);
      int src_pitch_uv = src->GetPitch(PLANAR_U);
      int chroma_pitch_uv = chroma->GetPitch(PLANAR_U);
      int src_rowsize_u = src->GetRowSize(PLANAR_U);
      int src_rowsize_v = src->GetRowSize(PLANAR_V);
      int src_height_uv = src->GetHeight(PLANAR_U);

      merge_plane(srcpU, chromapU, src_pitch_uv, chroma_pitch_uv, src_rowsize_u, src_height_uv, weight, pixelsize,
                  bits_per_pixel, plan_.get(), env);
      merge_plane(srcpV, chromapV, src_pitch_uv, chroma_pitch_uv, src_rowsize_v, src_height_uv, weight, pixelsize,
                  bits_per_pixel, plan_.get(), env);

      if (vi.IsYUVA())
        merge_plane(src->GetWritePtr(PLANAR_A), chroma->GetReadPtr(PLANAR_A), src->GetPitch(PLANAR_A),
                    chroma->GetPitch(PLANAR_A), src->GetRowSize(PLANAR_A), src->GetHeight(PLANAR_A), weight, pixelsize,
                    bits_per_pixel, plan_.get(), env);
    }
  } else { // weight == 1.0
    if (vi.IsYUY2()) {
      env->MakeWritable(&chroma);
      mix({chroma->GetWritePtr(), chroma->GetPitch(), 2}, {src->GetReadPtr(), src->GetPitch(), 2}, {w / 2, h, 0, h}, 8,
          1.0, plan_.get(), env);
      return chroma;
    } else {
      if (src->IsWritable()) {
        src->GetWritePtr(PLANAR_Y); //Must be requested
        env->BitBlt(src->GetWritePtr(PLANAR_U), src->GetPitch(PLANAR_U), chroma->GetReadPtr(PLANAR_U),
                    chroma->GetPitch(PLANAR_U), chroma->GetRowSize(PLANAR_U), chroma->GetHeight(PLANAR_U));
        env->BitBlt(src->GetWritePtr(PLANAR_V), src->GetPitch(PLANAR_V), chroma->GetReadPtr(PLANAR_V),
                    chroma->GetPitch(PLANAR_V), chroma->GetRowSize(PLANAR_V), chroma->GetHeight(PLANAR_V));
        if (vi.IsYUVA())
          env->BitBlt(src->GetWritePtr(PLANAR_A), src->GetPitch(PLANAR_A), chroma->GetReadPtr(PLANAR_A),
                      chroma->GetPitch(PLANAR_A), chroma->GetRowSize(PLANAR_A), chroma->GetHeight(PLANAR_A));
      } else { // avoid the cost of 2 chroma blits
        PVideoFrame dst = env->NewVideoFrameP(vi, &src);

        env->BitBlt(dst->GetWritePtr(PLANAR_Y), dst->GetPitch(PLANAR_Y), src->GetReadPtr(PLANAR_Y),
                    src->GetPitch(PLANAR_Y), src->GetRowSize(PLANAR_Y), src->GetHeight(PLANAR_Y));
        env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), chroma->GetReadPtr(PLANAR_U),
                    chroma->GetPitch(PLANAR_U), chroma->GetRowSize(PLANAR_U), chroma->GetHeight(PLANAR_U));
        env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), chroma->GetReadPtr(PLANAR_V),
                    chroma->GetPitch(PLANAR_V), chroma->GetRowSize(PLANAR_V), chroma->GetHeight(PLANAR_V));
        if (vi.IsYUVA())
          env->BitBlt(dst->GetWritePtr(PLANAR_A), dst->GetPitch(PLANAR_A), chroma->GetReadPtr(PLANAR_A),
                      chroma->GetPitch(PLANAR_A), chroma->GetRowSize(PLANAR_A), chroma->GetHeight(PLANAR_A));

        return dst;
      }
    }
  }
  return src;
}

AVSValue __cdecl MergeChroma::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new MergeChroma(args[0].AsClip(), args[1].AsClip(), (float)args[2].AsFloat(1.0f), env);
}
} // namespace aif::filters::merge
