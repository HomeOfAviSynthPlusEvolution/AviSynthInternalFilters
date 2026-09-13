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

#include "weave_columns.h"
#include "kernel_adapter.h"
#include <limits>
namespace aif::filters::rows_columns {
WeaveColumns::WeaveColumns(PClip _child, int _period, IScriptEnvironment* env)
    : GenericVideoFilter(_child), period(_period), inframes(vi.num_frames) {
  if (_period <= 0)
    env->ThrowError("WeaveColumns: period must be greater than zero.");

  if (vi.width > std::numeric_limits<int>::max() / _period)
    env->ThrowError("WeaveColumns: Maximum dimension exceeded.");
  vi.width *= _period;
  vi.MulDivFPS(1, _period);
  vi.num_frames = int((int64_t(vi.num_frames) + _period - 1) / _period);
}

PVideoFrame WeaveColumns::GetFrame(int n, IScriptEnvironment* env) {
  std::vector<PVideoFrame> frames;
  frames.reserve(period);
  for (int m = 0; m < period; ++m)
    frames.push_back(child->GetFrame(std::min(int64_t(n) * period + m, int64_t(inframes - 1)), env));
  PVideoFrame dst = env->NewVideoFrameP(vi, &frames[0]);
  column_frame(frames, dst, vi, period, 0, true, env);
  return dst;
}
AVSValue __cdecl WeaveColumns::Create(AVSValue args, void*, IScriptEnvironment* env) {
  if (args[1].AsInt() == 1)
    return args[0];

  return new WeaveColumns(args[0].AsClip(), args[1].AsInt(), env);
}
} // namespace aif::filters::rows_columns
