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

#include "skew_rows.h"
#include "kernel/correction.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <avs/minmax.h>
namespace aif::filters::legacy_correction {
SkewRows::SkewRows(PClip _child, int skew, IScriptEnvironment* env) : GenericVideoFilter(_child) {
  if ((vi.NumComponents() > 1) && vi.IsPlanar())
    env->ThrowError("SkewRows: requires non-planar or greyscale input");

  if (vi.IsYUY2() && skew & 1)
    env->ThrowError("SkewRows: For YUY2 skew must be even");

  if (skew <= -vi.width)
    env->ThrowError("SkewRows: output width must be positive");

  const int64_t width = int64_t(vi.width) + skew;
  const int64_t height = (int64_t(vi.width) * vi.height + width - 1) / width;
  if (width > INT32_MAX || height > INT32_MAX)
    env->ThrowError("SkewRows: output dimensions exceed integer range.");
  vi.width = int(width);
  vi.height = int(height);
}
PVideoFrame SkewRows::GetFrame(int n, IScriptEnvironment* env) {

  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrameP(vi, &src);

  skew(dst->GetWritePtr(), src->GetReadPtr(), dst->GetPitch(), src->GetPitch(), dst->GetRowSize(), src->GetRowSize(),
       src->GetHeight());
  return dst;
}
AVSValue __cdecl SkewRows::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new SkewRows(args[0].AsClip(), args[1].AsInt(), env);
}
} // namespace aif::filters::legacy_correction
