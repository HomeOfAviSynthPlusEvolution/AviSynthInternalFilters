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

#include "field_operations.h"
#include "complement_parity.h"
#include "assume_parity.h"
#include "assume_field_based.h"
#include "assume_frame_based.h"
#include "separate_fields.h"
#include "double_weave_fields.h"
#include "double_weave_frames.h"
#include "fieldwise.h"
namespace aif::filters::field {
AVSValue __cdecl Create_DoubleWeave(AVSValue args, void*, IScriptEnvironment* env) {
  (void)env;
  PClip clip = args[0].AsClip();
  if (clip->GetVideoInfo().IsFieldBased())
    return new DoubleWeaveFields(clip, env);
  else
    return new DoubleWeaveFrames(clip);
}
AVSValue __cdecl Create_Weave(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (!clip->GetVideoInfo().IsFieldBased())
    env->ThrowError("Weave: Weave should be applied on field-based material: use AssumeFieldBased() beforehand");
  return env->Invoke("SelectEven", Create_DoubleWeave(args, 0, env));
}
AVSValue __cdecl Create_Pulldown(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  AVSValue selectargs[] = {clip, 5, args[1].AsInt() % 5, args[2].AsInt() % 5};
  return new AssumeFrameBased(env->Invoke("SelectEvery", AVSValue(selectargs, 4)).AsClip());
}
AVSValue __cdecl Create_SwapFields(AVSValue args, void*, IScriptEnvironment* env) {
  return env->Invoke(
      "SelectEven", PClip(new DoubleWeaveFields(new ComplementParity(new SeparateFields(args[0].AsClip(), env)), env)));
}
AVSValue __cdecl Create_Bob(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (!clip->GetVideoInfo().IsFieldBased())
    clip = new SeparateFields(clip, env);

  const VideoInfo& vi = clip->GetVideoInfo();

  if (vi.height > INT32_MAX / 2)
    env->ThrowError("Bob: Output height exceeds integer range.");
  const double b = args[1].AsFloat(1. / 3.), c = args[2].AsFloat(1. / 3.);
  const int height = args[3].AsInt(vi.height * 2);
  AVSValue lower[] = {clip, vi.width, height, b, c, 0.0, -0.25, double(vi.width), double(vi.height)};
  AVSValue upper[] = {clip, vi.width, height, b, c, 0.0, +0.25, double(vi.width), double(vi.height)};
  return new Fieldwise(env->Invoke("BicubicResize", AVSValue(lower, 9)).AsClip(),
                       env->Invoke("BicubicResize", AVSValue(upper, 9)).AsClip());
}
} // namespace aif::filters::field
