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

#include "blur_sharpen.h"
#include "adjust_focus_h.h"
#include "adjust_focus_v.h"
#include <cmath>

namespace aif::filters::focus {
AVSValue __cdecl Create_Sharpen(AVSValue args, void*, IScriptEnvironment* env) {
  const double amountH = args[1].AsFloat(), amountV = args[2].AsDblDef(amountH);

  if (!std::isfinite(amountH) || !std::isfinite(amountV) || amountH < -1.5849625 || amountH > 1.0 ||
      amountV < -1.5849625 || amountV > 1.0) // log2(3)
    env->ThrowError("Sharpen: arguments must be in the range -1.58 to 1.0");

  if (fabs(amountH) < 0.00002201361136) { // log2(1+1/65536)
    if (fabs(amountV) < 0.00002201361136) {
      return args[0].AsClip();
    } else {
      return new AdjustFocusV(amountV, args[0].AsClip(), env);
    }
  } else {
    if (fabs(amountV) < 0.00002201361136) {
      return new AdjustFocusH(amountH, args[0].AsClip(), env);
    } else {
      return new AdjustFocusH(amountH, new AdjustFocusV(amountV, args[0].AsClip(), env), env);
    }
  }
}

AVSValue __cdecl Create_Blur(AVSValue args, void*, IScriptEnvironment* env) {
  const double amountH = args[1].AsFloat(), amountV = args[2].AsDblDef(amountH);

  if (!std::isfinite(amountH) || !std::isfinite(amountV) || amountH < -1.0 || amountH > 1.5849625 || amountV < -1.0 ||
      amountV > 1.5849625) // log2(3)
    env->ThrowError("Blur: arguments must be in the range -1.0 to 1.58");

  if (fabs(amountH) < 0.00002201361136) { // log2(1+1/65536)
    if (fabs(amountV) < 0.00002201361136) {
      return args[0].AsClip();
    } else {
      return new AdjustFocusV(-amountV, args[0].AsClip(), env);
    }
  } else {
    if (fabs(amountV) < 0.00002201361136) {
      return new AdjustFocusH(-amountH, args[0].AsClip(), env);
    } else {
      return new AdjustFocusH(-amountH, new AdjustFocusV(-amountV, args[0].AsClip(), env), env);
    }
  }
}

} // namespace aif::filters::focus
