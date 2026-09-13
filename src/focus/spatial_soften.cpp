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

#include "spatial_soften.h"
#include "kernel_adapter.h"

namespace aif::filters::focus {
SpatialSoften::SpatialSoften(PClip _child, int _radius, unsigned _luma_threshold, unsigned _chroma_threshold,
                             IScriptEnvironment* env)
    : GenericVideoFilter(_child), luma_threshold(_luma_threshold), chroma_threshold(_chroma_threshold), diameter(0) {
  if (!vi.IsYUY2())
    env->ThrowError("SpatialSoften: requires YUY2 input");
  if (_radius < 0 || _radius > 32)
    env->ThrowError("SpatialSoften: radius must be between 0 and 32");
  diameter = _radius * 2 + 1;
}

PVideoFrame SpatialSoften::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrameP(vi, &src);
  checked(aif_focus_spatial_yuy2(src->GetReadPtr(), src->GetPitch(), dst->GetWritePtr(), dst->GetPitch(),
                                 src->GetRowSize(), vi.height, diameter / 2, luma_threshold, chroma_threshold),
          env);
  return dst;
}

AVSValue __cdecl SpatialSoften::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new SpatialSoften(args[0].AsClip(), args[1].AsInt(), args[2].AsInt(), args[3].AsInt(), env);
}

} // namespace aif::filters::focus
