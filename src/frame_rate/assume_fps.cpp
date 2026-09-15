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

#include <avisynth.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#ifdef AVS_WINDOWS
#include <avs/win.h>
#else
#include <avs/posix.h>
#endif
#include <avs/minmax.h>
#include "rational.h"
#include "assume_fps.h"
namespace aif::filters::frame_rate {
AssumeFPS::AssumeFPS(PClip _child, unsigned numerator, unsigned denominator, bool sync_audio, IScriptEnvironment* env)
    : NonCachedGenericVideoFilter(_child) {
  ValidateFPS("AssumeFPS: source", vi.fps_numerator, vi.fps_denominator, env);
  ValidateFPS("AssumeFPS: target", numerator, denominator, env);

  if (denominator == 0)
    env->ThrowError("AssumeFPS: Denominator cannot be 0 (zero).");

  if (sync_audio) {
    vi.audio_samples_per_second =
        ScaleAudioRate("AssumeFPS", vi.audio_samples_per_second, uint64_t(vi.fps_denominator) * numerator,
                       uint64_t(vi.fps_numerator) * denominator, env);
  }
  vi.SetFPS(numerator, denominator);
}

AVSValue __cdecl AssumeFPS::Create(AVSValue args, void*, IScriptEnvironment* env) {
  int numerator = args[1].AsInt();
  int denominator = args[2].AsInt(1);
  if (numerator <= 0 || denominator <= 0)
    env->ThrowError("AssumeFPS: numerator and denominator must be greater than zero");

  return new AssumeFPS(args[0].AsClip(), numerator, denominator, args[3].AsBool(false), env);
}

AVSValue __cdecl AssumeFPS::CreateFloat(AVSValue args, void*, IScriptEnvironment* env) {
  uint32_t num, den;

  FloatToFPS("AssumeFPS", args[1].AsFloatf(), num, den, env);
  return new AssumeFPS(args[0].AsClip(), num, den, args[2].AsBool(false), env);
}

// Tritical Jan 2006
AVSValue __cdecl AssumeFPS::CreatePreset(AVSValue args, void*, IScriptEnvironment* env) {
  uint32_t num, den;

  PresetToFPS("AssumeFPS", args[1].AsString(), num, den, env);
  return new AssumeFPS(args[0].AsClip(), num, den, args[2].AsBool(false), env);
}

AVSValue __cdecl AssumeFPS::CreateFromClip(AVSValue args, void*, IScriptEnvironment* env) {
  const VideoInfo& vi = args[1].AsClip()->GetVideoInfo();

  if (!vi.HasVideo()) {
    env->ThrowError("AssumeFPS: The clip supplied to get the FPS from must contain video.");
  }

  return new AssumeFPS(args[0].AsClip(), vi.fps_numerator, vi.fps_denominator, args[2].AsBool(false), env);
}

/************************************
 *******   ChangeFPS Filters   ******
 ************************************/

} // namespace aif::filters::frame_rate
