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

#include "separate_columns.h"
#include "kernel_adapter.h"
#include <limits>
namespace aif::filters::rows_columns {
SeparateColumns::SeparateColumns(PClip _child, int _interval, IScriptEnvironment* env)
    : GenericVideoFilter(_child), interval(_interval) {
  if (_interval <= 0)
    env->ThrowError("SeparateColumns: interval must be greater than zero.");

  if (_interval > vi.width)
    env->ThrowError("SeparateColumns: interval must be less than or equal width.");

  if (vi.width % _interval)
    env->ThrowError("SeparateColumns: width must be mod %d.", _interval);

  vi.width /= _interval;
  vi.MulDivFPS(_interval, 1);
  if (vi.num_frames > std::numeric_limits<int>::max() / _interval)
    env->ThrowError("SeparateColumns: Maximum number of frames exceeded.");
  vi.num_frames *= _interval;

  if (vi.num_frames < 0)
    env->ThrowError("SeparateColumns: Maximum number of frames exceeded.");

  if (vi.IsYUY2() && vi.width & 1)
    env->ThrowError("SeparateColumns: YUY2 output width must be even.");
  if (vi.Is420() && vi.width & 1)
    env->ThrowError("SeparateColumns: YUV420 output width must be even.");
  if (vi.Is422() && vi.width & 1)
    env->ThrowError("SeparateColumns: YUV422 output width must be even.");
  if (vi.IsYV411() && vi.width & 3)
    env->ThrowError("SeparateColumns: YV411 output width must be mod 4.");
}

PVideoFrame SeparateColumns::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n / interval, env), dst = env->NewVideoFrameP(vi, &src);
  column_frame({src}, dst, vi, interval, n % interval, false, env);
  return dst;
}
AVSValue __cdecl SeparateColumns::Create(AVSValue args, void*, IScriptEnvironment* env) {
  if (args[1].AsInt() == 1)
    return args[0];

  return new SeparateColumns(args[0].AsClip(), args[1].AsInt(), env);
}
} // namespace aif::filters::rows_columns
