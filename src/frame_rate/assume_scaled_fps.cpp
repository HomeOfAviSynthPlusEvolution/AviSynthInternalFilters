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
#include "assume_scaled_fps.h"
namespace aif::filters::frame_rate {
AssumeScaledFPS::AssumeScaledFPS(PClip _child, int multiplier, int divisor, bool sync_audio, IScriptEnvironment* env)
    : NonCachedGenericVideoFilter(_child) {
  if (divisor <= 0)
    env->ThrowError("AssumeScaledFPS: Divisor must be positive.");

  if (multiplier <= 0)
    env->ThrowError("AssumeScaledFPS: Multiplier must be positive.");

  ValidateFPS("AssumeScaledFPS: source", vi.fps_numerator, vi.fps_denominator, env);
  if (sync_audio) {
    vi.audio_samples_per_second =
        ScaleAudioRate("AssumeScaledFPS", vi.audio_samples_per_second, unsigned(multiplier), unsigned(divisor), env);
  }
  vi.MulDivFPS((uint32_t)multiplier, (uint32_t)divisor);
  ValidateFPS("AssumeScaledFPS: target", vi.fps_numerator, vi.fps_denominator, env);
}

AVSValue __cdecl AssumeScaledFPS::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new AssumeScaledFPS(args[0].AsClip(), args[1].AsInt(1), args[2].AsInt(1), args[3].AsBool(false), env);
}

/************************************
 *******   AssumeFPS Filters   ******
 ************************************/

} // namespace aif::filters::frame_rate
