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

#include "separate_fields.h"
#include <avs/minmax.h>
#include <algorithm>
namespace aif::filters::field {
SeparateFields::SeparateFields(PClip _child, IScriptEnvironment* env) : NonCachedGenericVideoFilter(_child) {
  if (vi.height & 1)
    env->ThrowError("SeparateFields: height must be even");
  if (vi.Is420() && vi.height & 3)
    env->ThrowError("SeparateFields: YUV420 height must be multiple of 4");
  vi.height >>= 1;
  vi.MulDivFPS(2, 1);
  if (vi.num_frames > INT32_MAX / 2)
    env->ThrowError("SeparateFields: Maximum number of frames exceeded.");
  vi.num_frames *= 2;

  if (vi.num_frames < 0)
    env->ThrowError("SeparateFields: Maximum number of frames exceeded.");

  vi.SetFieldBased(true);
}

PVideoFrame SeparateFields::GetFrame(int n, IScriptEnvironment* env) {
#ifdef CACHE_GROWTH_INFINITELY_TEST
  // FIXME: debug for Issue #270
  // See other occurencies of this define
  // When filter is combined with non-SeparateFielded frames
  // the cache can grow infinitely, behaves like a memory leak.
  // Tried putting n or (n-1) or (n*2) instead of n >> 1 then the problem does not occur.
  _RPT2(0, "SeparateFields::GetFrame before %d, >>1: %d\n", n, n >> 1);
#endif
  PVideoFrame frame = child->GetFrame(n >> 1, env);
#ifdef CACHE_GROWTH_INFINITELY_TEST
  _RPT2(0, "SeparateFields::GetFrame after %d, >>1: %d\n", n, n >> 1);
#endif
  if (vi.IsPlanar()) {
    const bool topfield = GetParity(n);

    int plane0 = vi.IsRGB() ? PLANAR_G : PLANAR_Y;
    int plane1 = vi.IsRGB() ? PLANAR_B : PLANAR_U;
    const int Ypitch = frame->GetPitch(plane0);
    const int UVpitch = frame->GetPitch(plane1);
    const int UVoffset = !topfield ? UVpitch : 0;
    const int Yoffset = !topfield ? Ypitch : 0;

    if (vi.NumComponents() == 4) {
      int Aoffset = !topfield ? frame->GetPitch(PLANAR_A) : 0;

      return env->SubframePlanarA(frame, Yoffset, frame->GetPitch() * 2, frame->GetRowSize(), frame->GetHeight() >> 1,
                                  UVoffset, UVoffset, frame->GetPitch(PLANAR_U) * 2, Aoffset);
    } else {
      return env->SubframePlanar(frame, Yoffset, frame->GetPitch() * 2, frame->GetRowSize(), frame->GetHeight() >> 1,
                                 UVoffset, UVoffset, frame->GetPitch(PLANAR_U) * 2);
    }
  }
  return env->Subframe(frame, (GetParity(n) ^ vi.IsYUY2()) ? frame->GetPitch() : 0, frame->GetPitch() * 2,
                       frame->GetRowSize(), frame->GetHeight() >> 1);
}

AVSValue __cdecl SeparateFields::Create(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (clip->GetVideoInfo().IsFieldBased())
    env->ThrowError(
        "SeparateFields: SeparateFields should be applied on frame-based material: use AssumeFrameBased() beforehand");

  return new SeparateFields(clip, env);
}
bool __stdcall SeparateFields::GetParity(int n) {
  return child->GetParity(n >> 1) ^ (n & 1);
}
} // namespace aif::filters::field
