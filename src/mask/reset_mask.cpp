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

// Avisynth filter: Layer
// by "poptones" (poptones@myrealbox.com)

#include "reset_mask.h"
#include "kernel_adapter.h"
#include "kernel/scalar_helpers.h"
namespace aif::filters::mask {
using std::min;
using std::clamp;
ResetMask::ResetMask(PClip _child, float _mask_f, IScriptEnvironment* env) : GenericVideoFilter(_child) {
  if (std::isnan(_mask_f))
    env->ThrowError("ResetMask: mask cannot be NaN");

  if (!(vi.IsRGB32() || vi.IsRGB64() || vi.IsPlanarRGBA() || vi.IsYUVA()))
    env->ThrowError("ResetMask: format has no alpha channel");

  // new: resetmask has parameter. If none->max transparency

  row_ = aif_mask_resolve(allowed_cpu(env));
  int max_pixel_value = vi.ComponentSize() == 4 ? 1 : (1 << vi.BitsPerComponent()) - 1;
  if (_mask_f < 0) {
    mask_f = 1.0f;
    mask = max_pixel_value;
  } else {
    mask_f = _mask_f;
    if (mask_f < 0)
      mask_f = 0;
    mask = static_cast<int>(std::clamp(double(mask_f), 0.0, double(max_pixel_value)));

    mask = clamp(mask, 0, max_pixel_value);
    mask_f = clamp(mask_f, 0.0f, 1.0f);
  }
}

PVideoFrame ResetMask::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame f = child->GetFrame(n, env);
  env->MakeWritable(&f);

  if (vi.IsPlanarRGBA() || vi.IsYUVA()) {
    const int dst_rowsizeA = f->GetRowSize(PLANAR_A);
    const int dst_pitchA = f->GetPitch(PLANAR_A);
    BYTE* dstp_a = f->GetWritePtr(PLANAR_A);
    const int heightA = f->GetHeight(PLANAR_A);

    switch (vi.ComponentSize()) {
      case 1:
        fill_plane<BYTE>(dstp_a, heightA, dst_rowsizeA, dst_pitchA, static_cast<BYTE>(mask));
        break;
      case 2:
        fill_plane<uint16_t>(dstp_a, heightA, dst_rowsizeA, dst_pitchA, mask);
        break;
      case 4:
        fill_plane<float>(dstp_a, heightA, dst_rowsizeA, dst_pitchA, mask_f);
        break;
    }
    return f;
  }
  // RGB32 and RGB64

  BYTE* pf = f->GetWritePtr();
  int pitch = f->GetPitch();
  int rowsize = f->GetRowSize();
  int height = f->GetHeight();

  if (vi.IsRGB32()) {
    for (int y = 0; y < height; y++) {
      row_(pf, nullptr, vi.width, 2, uint32_t(mask), 0);
      pf += pitch;
    }
  } else if (vi.IsRGB64()) {
    rowsize /= sizeof(uint16_t);
    for (int y = 0; y < height; y++) {
      for (int x = 3; x < rowsize; x += 4) {
        reinterpret_cast<uint16_t*>(pf)[x] = mask;
      }
      pf += pitch;
    }
  }

  return f;
}

AVSValue ResetMask::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new ResetMask(args[0].AsClip(), (float)args[1].AsFloat(-1.0f), env);
}

/********************************
 ******  Invert filter  ******
 ********************************/

} // namespace aif::filters::mask
