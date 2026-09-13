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

/*
** Turn. version 0.1
** (c) 2003 - Ernst Peché
**
*/

#include "flip_vertical.h"
#include "kernel_adapter.h"
namespace aif::filters::rotation {
PVideoFrame __stdcall FlipVertical::GetFrame(int n, IScriptEnvironment* env) {
  auto s = child->GetFrame(n, env);
  auto d = env->NewVideoFrameP(vi, &s);
  const int planes[] = {0, vi.IsRGB() ? PLANAR_B : PLANAR_U, vi.IsRGB() ? PLANAR_R : PLANAR_V, PLANAR_A};
  for (int i = 0; i < (vi.IsPlanar() ? vi.NumComponents() : 1); ++i)
    apply(s, d, planes[i], planes[i], pixel_bytes(vi), 4, env);
  return d;
}
AVSValue __cdecl FlipVertical::Create(AVSValue args, void*, IScriptEnvironment*) {
  return new FlipVertical(args[0].AsClip());
}
} // namespace aif::filters::rotation
