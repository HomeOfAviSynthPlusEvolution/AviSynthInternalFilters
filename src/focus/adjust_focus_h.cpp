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

#include "adjust_focus_h.h"
#include "kernel_adapter.h"
#include <cmath>

namespace aif::filters::focus {
static int layout(const VideoInfo& vi) {
  if (vi.IsPlanar())
    return AIF_FOCUS_PLANAR;
  if (vi.IsYUY2())
    return AIF_FOCUS_YUY2;
  return vi.IsRGB24() || vi.IsRGB48() ? AIF_FOCUS_RGB3 : AIF_FOCUS_RGB4;
}

AdjustFocusH::AdjustFocusH(double _amount, PClip _child, IScriptEnvironment* env)
    : GenericVideoFilter(_child), cpu_mask_(allowed_cpu(env)), amountd(pow(2.0, _amount)) {
  half_amount = int(32768 * amountd + 0.5);
}

PVideoFrame __stdcall AdjustFocusH::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrameP(vi, &src);
  const int yuv[] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
  const int rgb[] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
  const int* planes = vi.IsPlanarRGB() || vi.IsPlanarRGBA() ? rgb : yuv;
  const int count = vi.IsPlanar() ? vi.NumComponents() : 1;
  for (int i = 0; i < count; ++i) {
    const int p = vi.IsPlanar() ? planes[i] : 0;
    if (i == 3) {
      env->BitBlt(dst->GetWritePtr(p), dst->GetPitch(p), src->GetReadPtr(p), src->GetPitch(p), src->GetRowSize(p),
                  src->GetHeight(p));
      continue;
    }
    checked(aif_focus_horizontal(src->GetReadPtr(p), src->GetPitch(p), dst->GetWritePtr(p), dst->GetPitch(p),
                                 src->GetRowSize(p), src->GetHeight(p), vi.BitsPerComponent(), layout(vi), half_amount,
                                 static_cast<float>(amountd), cpu_mask_),
            env);
  }
  return dst;
}

} // namespace aif::filters::focus
