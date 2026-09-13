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

#include "adjust_focus_v.h"
#include "kernel_adapter.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace aif::filters::focus {
AdjustFocusV::AdjustFocusV(double _amount, PClip _child) : GenericVideoFilter(_child), amountd(pow(2.0, _amount)) {
  half_amount = int(32768 * amountd + 0.5);
}

PVideoFrame __stdcall AdjustFocusV::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame frame = child->GetFrame(n, env);
  env->MakeWritable(&frame);
  const size_t scratch_size = (static_cast<size_t>(frame->GetRowSize()) + 31) & ~size_t(31);
  void* scratch = env->Allocate(scratch_size, 32, AVS_POOLED_ALLOC);
  if (!scratch)
    env->ThrowError("AdjustFocusV: Could not reserve memory.");
  struct FreeScratch {
    IScriptEnvironment* env;
    void operator()(void* p) const { env->Free(p); }
  };
  std::unique_ptr<void, FreeScratch> owner(scratch, FreeScratch{env});
  const int yuv[] = {PLANAR_Y, PLANAR_U, PLANAR_V};
  const int rgb[] = {PLANAR_G, PLANAR_B, PLANAR_R};
  const int* planes = vi.IsPlanarRGB() || vi.IsPlanarRGBA() ? rgb : yuv;
  const int count = vi.IsPlanar() ? std::min(vi.NumComponents(), 3) : 1;
  for (int i = 0; i < count; ++i) {
    const int p = vi.IsPlanar() ? planes[i] : 0;
    checked(aif_focus_vertical(frame->GetWritePtr(p), frame->GetPitch(p), frame->GetRowSize(p), frame->GetHeight(p),
                               vi.BitsPerComponent(), half_amount, static_cast<float>(amountd), scratch, scratch_size,
                               allowed_cpu(env)),
            env);
  }
  return frame;
}

} // namespace aif::filters::focus
