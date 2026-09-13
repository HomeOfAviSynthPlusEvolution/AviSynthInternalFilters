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

#include "weave_rows.h"
#include "kernel_adapter.h"
#include <limits>
namespace aif::filters::rows_columns {
WeaveRows::WeaveRows(PClip _child, int _period, IScriptEnvironment* env)
    : GenericVideoFilter(_child), period(_period), inframes(vi.num_frames) {
  if (_period <= 0)
    env->ThrowError("WeaveRows: period must be greater than zero.");

  if (vi.height > std::numeric_limits<int>::max() / _period)
    env->ThrowError("WeaveRows: Maximum dimension exceeded.");
  vi.height *= _period;
  vi.MulDivFPS(1, _period);
  vi.num_frames = int((int64_t(vi.num_frames) + _period - 1) / _period);
}

PVideoFrame WeaveRows::GetFrame(int n, IScriptEnvironment* env) {
  const int64_t b = int64_t(n) * period;
  const int64_t e = b + period;

  PVideoFrame dst = env->NewVideoFrame(vi);
  BYTE* dstp = dst->GetWritePtr();
  const int dstpitch = dst->GetPitch();

  if (vi.IsRGB() && !vi.IsPlanar()) { // RGB upsidedown
    dstp += dstpitch * period;
    for (int64_t i = b; i < e; i++) {
      dstp -= dstpitch;
      const int j = i < inframes ? int(i) : inframes - 1;
      PVideoFrame src = child->GetFrame(j, env);
      if (i == b) // very first
        env->copyFrameProps(src, dst);

      env->BitBlt(dstp, dstpitch * period, src->GetReadPtr(), src->GetPitch(), src->GetRowSize(), src->GetHeight());
    }
  } else {
    int planes_y[4] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
    int planes_r[4] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
    int* planes = (vi.IsYUV() || vi.IsYUVA()) ? planes_y : planes_r;
    bool isYUY2 = vi.IsYUY2();
    int dstpitch[4];
    BYTE* dstp[4];
    for (int p = 0; p < (isYUY2 ? 1 : vi.NumComponents()); ++p) {
      int plane = planes[p];
      dstpitch[p] = dst->GetPitch(plane);
      dstp[p] = dst->GetWritePtr(plane);
    }

    for (int64_t i = b; i < e; i++) {
      const int j = i < inframes ? int(i) : inframes - 1;
      PVideoFrame src = child->GetFrame(j, env);
      if (i == b) // very first
        env->copyFrameProps(src, dst);
      for (int p = 0; p < (isYUY2 ? 1 : vi.NumComponents()); ++p) {
        int plane = planes[p];
        env->BitBlt(dstp[p], dstpitch[p] * period, src->GetReadPtr(plane), src->GetPitch(plane), src->GetRowSize(plane),
                    src->GetHeight(plane));
        dstp[p] += dstpitch[p];
      }
    }
  }
  return dst;
}

AVSValue __cdecl WeaveRows::Create(AVSValue args, void*, IScriptEnvironment* env) {
  if (args[1].AsInt() == 1)
    return args[0];

  return new WeaveRows(args[0].AsClip(), args[1].AsInt(), env);
}
} // namespace aif::filters::rows_columns
