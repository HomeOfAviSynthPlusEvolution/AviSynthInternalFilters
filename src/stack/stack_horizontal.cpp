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

#include "stack_horizontal.h"
#include <algorithm>
#include <cstring>
#include <blank_clip/kernel.h>
namespace aif::filters::stack {
StackHorizontal::StackHorizontal(const std::vector<PClip>& child_array, IScriptEnvironment* env)
    : children(child_array) {
  vi = children[0]->GetVideoInfo();

  for (size_t i = 1; i < children.size(); ++i) {
    const VideoInfo& vin = children[i]->GetVideoInfo();

    if (vi.height != vin.height)
      env->ThrowError("StackHorizontal: image heights don't match");

    if (!vi.IsSameColorspace(vin))
      env->ThrowError("StackHorizontal: image formats don't match");

    if (vi.num_frames < vin.num_frames) // Max of all clips
      vi.num_frames = vin.num_frames;

    vi.width += vin.width;
  }
}

PVideoFrame __stdcall StackHorizontal::GetFrame(int n, IScriptEnvironment* env) {
  std::vector<PVideoFrame> frames;
  frames.reserve(children.size());

  for (const auto& child : children)
    frames.emplace_back(child->GetFrame(n, env));

  PVideoFrame dst = env->NewVideoFrameP(vi, &frames[0]);
  const int dst_pitch = dst->GetPitch();
  const int height = dst->GetHeight();

  BYTE* dstp = dst->GetWritePtr();
  for (const auto& src : frames) {
    const int src_rowsize = src->GetRowSize();
    env->BitBlt(dstp, dst_pitch, src->GetReadPtr(), src->GetPitch(), src_rowsize, height);
    dstp += src_rowsize;
  }

  if (vi.IsPlanar() && (vi.NumComponents() > 1)) {
    // Copy Planar

    const int planesYUV[4] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
    const int planesRGB[4] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
    const int* planes = vi.IsYUV() || vi.IsYUVA() ? planesYUV : planesRGB;

    // first plane is already processed
    for (int p = 1; p < vi.NumComponents(); p++) {
      const int plane = planes[p];
      dstp = dst->GetWritePtr(plane);
      const int dst_pitch = dst->GetPitch(plane);
      const int height = dst->GetHeight(plane);

      for (const auto& src : frames) {
        const int src_rowsize = src->GetRowSize(plane);
        env->BitBlt(dstp, dst_pitch, src->GetReadPtr(plane), src->GetPitch(plane), src_rowsize, height);
        dstp += src_rowsize;
      }
    }
  }

  return dst;
}

AVSValue __cdecl StackHorizontal::Create(AVSValue args, void*, IScriptEnvironment* env) {
  if (args[1].IsArray()) {
    std::vector<PClip> children(1 + args[1].ArraySize());

    children[0] = args[0].AsClip();
    for (int i = 1; i < (int)children.size(); ++i) // Copy clips
      children[i] = args[1][i - 1].AsClip();

    return new StackHorizontal(children, env);
  } else if (args[1].IsClip()) { // Make easy to call with trivial 2 clips
    std::vector<PClip> children(2);

    children[0] = args[0].AsClip();
    children[1] = args[1].AsClip();

    return new StackHorizontal(children, env);
  } else {
    env->ThrowError("StackHorizontal: clip array not recognized!");
    return 0;
  }
}
} // namespace aif::filters::stack
