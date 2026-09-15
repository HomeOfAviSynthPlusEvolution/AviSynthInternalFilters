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

#include "show_five_versions.h"
#include <algorithm>
#include <cstring>
#include "kernel_adapter.h"
namespace aif::filters::stack {
ShowFiveVersions::ShowFiveVersions(PClip* children, IScriptEnvironment* env) : cpu_mask_(allowed_cpu(env)) {
  for (int b = 0; b < 5; ++b)
    child[b] = children[b];

  vi = child[0]->GetVideoInfo();

  for (int c = 1; c < 5; ++c) {
    const VideoInfo& viprime = child[c]->GetVideoInfo();
    vi.num_frames = std::max(vi.num_frames, viprime.num_frames);
    if (vi.width != viprime.width || vi.height != viprime.height || vi.pixel_type != viprime.pixel_type)
      env->ThrowError("ShowFiveVersions: video attributes of all clips must match");
  }

  vi.width *= 3;
  vi.height *= 2;
}

PVideoFrame __stdcall ShowFiveVersions::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame first = child[0]->GetFrame(n, env), dst = env->NewVideoFrameP(vi, &first);
  const int yuv[] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A}, rgb[] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
  const int* planes = vi.IsRGB() ? rgb : yuv;
  int count = vi.IsPlanar() ? vi.NumComponents() : 1;
  const uint32_t cpu = cpu_mask_;
  for (int p = 0; p < count; ++p) {
    int plane = vi.IsPlanar() ? planes[p] : 0;
    uint32_t pattern = 128;
    int bytes = vi.ComponentSize();
    if (bytes == 2)
      pattern = 128u << (vi.BitsPerComponent() - 8);
    if (bytes == 4) {
      float value = (!vi.IsRGB() && (plane == PLANAR_U || plane == PLANAR_V)) ? 0.f : 128.f / 255.f;
      std::memcpy(&pattern, &value, 4);
    }
    if (aif_blank_clip_fill(dst->GetWritePtr(plane), dst->GetPitch(plane), dst->GetRowSize(plane),
                            dst->GetHeight(plane), &pattern, bytes, cpu))
      env->ThrowError("ShowFiveVersions: fill failed");
  }
  for (int c = 0; c < 5; ++c) {
    PVideoFrame src = c == 0 ? first : child[c]->GetFrame(n, env);
    for (int p = 0; p < count; ++p) {
      int plane = vi.IsPlanar() ? planes[p] : 0;
      int row = src->GetRowSize(plane), height = src->GetHeight(plane), pitch = dst->GetPitch(plane);
      int half = vi.IsPlanar() ? (row / vi.ComponentSize() / 2) * vi.ComponentSize()
                               : vi.BytesFromPixels(child[c]->GetVideoInfo().width / 2);
      int bottom = (c & 1) ^ (vi.IsRGB() && !vi.IsPlanar());
      uint8_t* d = dst->GetWritePtr(plane) + (c / 2) * row + (c & 1 ? half : 0) + (bottom ? height * pitch : 0);
      env->BitBlt(d, pitch, src->GetReadPtr(plane), src->GetPitch(plane), row, height);
    }
  }
  return dst;
}
AVSValue __cdecl ShowFiveVersions::Create(AVSValue args, void*, IScriptEnvironment* env) {
  PClip children[5];
  for (int i = 0; i < 5; ++i)
    children[i] = args[i].AsClip();
  return new ShowFiveVersions(children, env);
}
} // namespace aif::filters::stack
