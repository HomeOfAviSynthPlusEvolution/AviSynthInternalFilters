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
#include "change_fps.h"
namespace aif::filters::frame_rate {
ChangeFPS::ChangeFPS(PClip _child, unsigned new_numerator, unsigned new_denominator, bool _linear,
                     IScriptEnvironment* env)
    : GenericVideoFilter(_child), linear(_linear) {
  ValidateFPS("ChangeFPS: source", vi.fps_numerator, vi.fps_denominator, env);
  ValidateFPS("ChangeFPS: target", new_numerator, new_denominator, env);

  if (new_numerator == 0 || new_denominator == 0)
    env->ThrowError("ChangeFPS: Numerator and denominator must be positive.");
  if (vi.fps_numerator == 0 || vi.fps_denominator == 0)
    env->ThrowError("ChangeFPS: Source frame rate must be positive.");

  FrameRatio("ChangeFPS", vi.fps_numerator, vi.fps_denominator, new_numerator, new_denominator, a, b, env);
  if (linear && (a + (b >> 1)) / b > 10)
    env->ThrowError("ChangeFPS: Ratio must be less than 10 for linear access. Set LINEAR=False.");

  vi.SetFPS(new_numerator, new_denominator);
  const int64_t num_frames = (vi.num_frames * b + (a >> 1)) / a;
  if (num_frames > 0x7FFFFFFF) // MAXINT
    env->ThrowError("ChangeFPS: Maximum number of frames exceeded.");

  vi.num_frames = int(num_frames);
  lastframe = -1;
}

PVideoFrame __stdcall ChangeFPS::GetFrame(int n, IScriptEnvironment* env) {
  int getframe = int((n * a) / b); // Use Floor! - Which frame to get next?

  if (linear) {
    if ((lastframe < (getframe - 1)) && (getframe - lastframe < 10)) { // Do not decode more than 10 frames
      while (lastframe < (getframe - 1)) {
        lastframe++;
        PVideoFrame p = child->GetFrame(lastframe, env); // If MSVC optimizes this I'll kill it ;)
      }
    }
  }

  lastframe = getframe;
  return child->GetFrame(getframe, env);
}

bool __stdcall ChangeFPS::GetParity(int n) {
  return child->GetParity(int((n * a) / b)); // Use Floor!
}

AVSValue __cdecl ChangeFPS::Create(AVSValue args, void*, IScriptEnvironment* env) {
  if (args[1].AsInt() <= 0 || args[2].AsInt(1) <= 0)
    env->ThrowError("ChangeFPS: Numerator and denominator must be positive.");
  return new ChangeFPS(args[0].AsClip(), args[1].AsInt(), args[2].AsInt(1), args[3].AsBool(true), env);
}

AVSValue __cdecl ChangeFPS::CreateFloat(AVSValue args, void*, IScriptEnvironment* env) {
  uint32_t num, den;

  FloatToFPS("ChangeFPS", args[1].AsFloatf(), num, den, env);
  return new ChangeFPS(args[0].AsClip(), num, den, args[2].AsBool(true), env);
}

// Tritical Jan 2006
AVSValue __cdecl ChangeFPS::CreatePreset(AVSValue args, void*, IScriptEnvironment* env) {
  uint32_t num, den;

  PresetToFPS("ChangeFPS", args[1].AsString(), num, den, env);
  return new ChangeFPS(args[0].AsClip(), num, den, args[2].AsBool(true), env);
}

AVSValue __cdecl ChangeFPS::CreateFromClip(AVSValue args, void*, IScriptEnvironment* env) {
  const VideoInfo& vi = args[1].AsClip()->GetVideoInfo();

  if (!vi.HasVideo()) {
    env->ThrowError("ChangeFPS: The clip supplied to get the FPS from must contain video.");
  }

  return new ChangeFPS(args[0].AsClip(), vi.fps_numerator, vi.fps_denominator, args[2].AsBool(true), env);
}

/*************************************
 *******   ConvertFPS Filters   ******
 *************************************/

} // namespace aif::filters::frame_rate
