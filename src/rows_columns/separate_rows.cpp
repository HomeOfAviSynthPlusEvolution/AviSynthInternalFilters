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

#include "separate_rows.h"
#include "kernel_adapter.h"
#include <limits>
namespace aif::filters::rows_columns {
SeparateRows::SeparateRows(PClip _child, int _interval, IScriptEnvironment* env)
    : GenericVideoFilter(_child), interval(_interval) {
  if (_interval <= 0)
    env->ThrowError("SeparateRows: interval must be greater than zero.");

  if (_interval > vi.height)
    env->ThrowError("SeparateRows: interval must be less than or equal height.");

  if (vi.height % _interval)
    env->ThrowError("SeparateRows: height must be mod %d.", _interval);

  vi.height /= _interval;
  vi.MulDivFPS(_interval, 1);
  if (vi.num_frames > std::numeric_limits<int>::max() / _interval)
    env->ThrowError("SeparateRows: Maximum number of frames exceeded.");
  vi.num_frames *= _interval;

  if (vi.num_frames < 0)
    env->ThrowError("SeparateRows: Maximum number of frames exceeded.");

  if (vi.Is420() && vi.height & 1)
    env->ThrowError("SeparateRows: YUV420 output height must be even.");
}

PVideoFrame SeparateRows::GetFrame(int n, IScriptEnvironment* env) {
  const int m = (vi.IsRGB() && !vi.IsPlanar()) ? interval - 1 - n % interval : n % interval; // RGB upside-down
  const int f = n / interval;

  PVideoFrame frame = child->GetFrame(f, env);

  if (vi.IsPlanar() && !vi.IsY()) {
    int plane0 = vi.IsRGB() ? PLANAR_G : PLANAR_Y;
    int plane1 = vi.IsRGB() ? PLANAR_B : PLANAR_U;
    const int Ypitch = frame->GetPitch(plane0);
    const int UVpitch = frame->GetPitch(plane1);
    const int Yoffset = Ypitch * m;
    const int UVoffset = UVpitch * m;

    if (vi.NumComponents() == 4) {
      int Aoffset = frame->GetPitch(PLANAR_A) * m;

      return env->SubframePlanarA(frame, Yoffset, Ypitch * interval, frame->GetRowSize(plane0), vi.height, UVoffset,
                                  UVoffset, UVpitch * interval, Aoffset);
    } else {
      return env->SubframePlanar(frame, Yoffset, Ypitch * interval, frame->GetRowSize(plane0), vi.height, UVoffset,
                                 UVoffset, UVpitch * interval);
    }
  }
  const int pitch = frame->GetPitch();
  return env->Subframe(frame, pitch * m, pitch * interval, frame->GetRowSize(), vi.height);
}

AVSValue __cdecl SeparateRows::Create(AVSValue args, void*, IScriptEnvironment* env) {
  if (args[1].AsInt() == 1)
    return args[0];

  return new SeparateRows(args[0].AsClip(), args[1].AsInt(), env);
}
} // namespace aif::filters::rows_columns
