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

#include "merge_all.h"
#include "kernel_adapter.h"
#include <cmath>
namespace aif::filters::merge {
MergeAll::MergeAll(PClip _child, PClip _clip, float _weight, IScriptEnvironment* env)
    : GenericVideoFilter(_child), clip(_clip), weight(_weight) {
  if (std::isnan(_weight))
    env->ThrowError("Merge: weight cannot be NaN");

  const VideoInfo& vi2 = clip->GetVideoInfo();

  if (!vi.IsSameColorspace(vi2))
    env->ThrowError("Merge: Pixel types are not the same. Both must be the same.");

  if (vi.width != vi2.width || vi.height != vi2.height)
    env->ThrowError("Merge: Images must have same width and height!");

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();

  if (weight < 0.0f)
    weight = 0.0f;
  if (weight > 1.0f)
    weight = 1.0f;
  plan_ = make_plan(env);
}

PVideoFrame __stdcall MergeAll::GetFrame(int n, IScriptEnvironment* env) {
  if (weight < 0.0039f)
    return child->GetFrame(n, env);
  if (weight > 0.9961f)
    return clip->GetFrame(n, env);

  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame src2 = clip->GetFrame(n, env);

  env->MakeWritable(&src);
  BYTE* srcp = src->GetWritePtr();
  const BYTE* srcp2 = src2->GetReadPtr();

  const int src_pitch = src->GetPitch();
  const int src_rowsize = src->GetRowSize();

  merge_plane(srcp, srcp2, src_pitch, src2->GetPitch(), src_rowsize, src->GetHeight(), weight, pixelsize,
              bits_per_pixel, plan_.get(), env);

  if (vi.IsPlanar()) {
    const int planesYUV[4] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
    const int planesRGB[4] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
    const int* planes = (vi.IsYUV() || vi.IsYUVA()) ? planesYUV : planesRGB;
    // first plane is already processed
    for (int p = 1; p < vi.NumComponents(); p++) {
      const int plane = planes[p];
      merge_plane(src->GetWritePtr(plane), src2->GetReadPtr(plane), src->GetPitch(plane), src2->GetPitch(plane),
                  src->GetRowSize(plane), src->GetHeight(plane), weight, pixelsize, bits_per_pixel, plan_.get(), env);
    }
  }

  return src;
}

AVSValue __cdecl MergeAll::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new MergeAll(args[0].AsClip(), args[1].AsClip(), (float)args[2].AsFloat(0.5f), env);
}
} // namespace aif::filters::merge
