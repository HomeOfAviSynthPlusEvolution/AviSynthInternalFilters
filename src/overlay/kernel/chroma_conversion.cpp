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

// Overlay (c) 2003, 2004 by Klaus Post

#include "chroma_conversion.h"
#include <initializer_list>
namespace aif::filters::overlay {
static void convert(PVideoFrame& src, PVideoFrame& dst, int bytes, bool expand, bool vertical, Chroma kernel,
                    IScriptEnvironment* env) {
  for (int plane : {PLANAR_Y, PLANAR_A}) {
    if (dst->GetRowSize(plane))
      env->BitBlt(dst->GetWritePtr(plane), dst->GetPitch(plane), src->GetReadPtr(plane), src->GetPitch(plane),
                  dst->GetRowSize(plane), dst->GetHeight(plane));
  }
  for (int plane : {PLANAR_U, PLANAR_V}) {
    const auto& small = expand ? src : dst;
    kernel(dst->GetWritePtr(plane), src->GetReadPtr(plane), dst->GetPitch(plane), src->GetPitch(plane),
           small->GetRowSize(plane) / bytes, small->GetHeight(plane), bytes, expand, vertical);
  }
}
void Convert444FromYV12(PVideoFrame& src, PVideoFrame& dst, int bytes, int bits, IScriptEnvironment* env,
                        Chroma kernel) {
  (void)bits;
  convert(src, dst, bytes, true, true, kernel, env);
}
void Convert444FromYV16(PVideoFrame& src, PVideoFrame& dst, int bytes, int bits, IScriptEnvironment* env,
                        Chroma kernel) {
  (void)bits;
  convert(src, dst, bytes, true, false, kernel, env);
}
void Convert444ToYV12(PVideoFrame& src, PVideoFrame& dst, int bytes, int bits, IScriptEnvironment* env, Chroma kernel) {
  (void)bits;
  convert(src, dst, bytes, false, true, kernel, env);
}
void Convert444ToYV16(PVideoFrame& src, PVideoFrame& dst, int bytes, int bits, IScriptEnvironment* env, Chroma kernel) {
  (void)bits;
  convert(src, dst, bytes, false, false, kernel, env);
}
void ConvertYToYV12Chroma(BYTE* d, BYTE* s, int dp, int sp, int bytes, int w, int h, IScriptEnvironment* env,
                          Chroma kernel) {
  (void)env;
  kernel(d, s, dp, sp, w, h, bytes, false, true);
}
void ConvertYToYV16Chroma(BYTE* d, BYTE* s, int dp, int sp, int bytes, int w, int h, IScriptEnvironment* env,
                          Chroma kernel) {
  (void)env;
  kernel(d, s, dp, sp, w, h, bytes, false, false);
}
} // namespace aif::filters::overlay
