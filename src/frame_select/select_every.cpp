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

#include "select_every.h"
#include <avs/minmax.h>
#include <algorithm>
#include "interleave.h"
namespace aif::filters::frame_select {
SelectEvery::SelectEvery(PClip _child, int _every, int _from, IScriptEnvironment* env)
    : NonCachedGenericVideoFilter(_child), every(_every), from(_from) {
  if (_every <= 0)
    env->ThrowError("Parameter 'every' of SelectEvery must be greater than zero.");
  if (_from < 0 || _from >= _every || _from >= _child->GetVideoInfo().num_frames)
    env->ThrowError(
        "Parameter 'from' of SelectEvery must be less than 'every' and the number of frames in the source clip.");

  vi.MulDivFPS(1, every);
  vi.num_frames = (vi.num_frames - 1 - from) / every + 1;
}

AVSValue __cdecl SelectEvery::Create(AVSValue args, void*, IScriptEnvironment* env) {
  const int num_vals = args[2].ArraySize();
  if (num_vals <= 1)
    return new SelectEvery(args[0].AsClip(), args[1].AsInt(), num_vals > 0 ? args[2][0].AsInt() : 0, env);
  else {
    std::vector<PClip> children(num_vals);

    for (int i = 0; i < (int)children.size(); ++i)
      children[i] = new SelectEvery(args[0].AsClip(), args[1].AsInt(), args[2][i].AsInt(), env);

    return new Interleave(std::move(children), env);
  }
}
PVideoFrame __stdcall SelectEvery::GetFrame(int n, IScriptEnvironment* env) {
  return child->GetFrame(n * every + from, env);
}
bool __stdcall SelectEvery::GetParity(int n) {
  return child->GetParity(n * every + from);
}
AVSValue __cdecl SelectEvery::Create_SelectEven(AVSValue args, void*, IScriptEnvironment* env) {
  return new SelectEvery(args[0].AsClip(), 2, 0, env);
}
AVSValue __cdecl SelectEvery::Create_SelectOdd(AVSValue args, void*, IScriptEnvironment* env) {
  return new SelectEvery(args[0].AsClip(), 2, 1, env);
}
} // namespace aif::filters::frame_select
