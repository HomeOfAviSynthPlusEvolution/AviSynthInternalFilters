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

// FIXME: in general: how to display 32 bit floats?
// Do we have to assume it as if it is used after a limited -> full scale conversion? (preferred - everywhere!)
// Or with values simply: pixel_8bit / 255.0?
// Latter logic converts U=240 to (240-128)/255 instead of (240-128)/226 (=0.5)
// and Y=235 to 235/255 instead of (235-16)/219 (=1.0)

#include "histogram.h"
#include "support.h"
#include <algorithm>
#include <cstring>

#ifdef AVS_WINDOWS
#include <avs/win.h>
#else
#include <avs/posix.h>
#endif

#include <memory>
#include <avs/minmax.h>
#include <cstdio>
#include <cmath>
#include <stdint.h>

constexpr double PI = 3.14159265358979323846;
// until c++20 <numbers> std::numbers::pi

/********************************************************************
***** Declare index of new filters for Avisynth's filter engine *****
********************************************************************/

namespace aif::filters::histogram {
Histogram::Histogram(PClip _child, Mode _mode, AVSValue _option, int _show_bits, bool _keepsource, bool _markers,
                     IScriptEnvironment* env)
    : GenericVideoFilter(_child), mode(_mode), option(_option), show_bits(_show_bits), keepsource(_keepsource),
      markers(_markers) {
  bool optionValid = false;

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();

  if (show_bits < 8 || show_bits > 12)
    env->ThrowError("Histogram: bits parameter can only be 8, 9 .. 12");

  // until all histogram is ported
  bool non8bit = show_bits != 8 || bits_per_pixel != 8;

  if (non8bit && mode != ModeClassic && mode != ModeLevels && mode != ModeColor && mode != ModeColor2 &&
      mode != ModeLuma) {
    env->ThrowError("Histogram: this histogram type is available only for 8 bit formats and parameters");
  }

  origwidth = vi.width;
  origheight = vi.height;

  if (mode == ModeClassic) {
    if (!vi.IsYUV() && !vi.IsYUVA())
      env->ThrowError("Histogram: YUV(A) data only");
    if (keepsource)
      vi.width += (1 << show_bits);
    else
      vi.width = (1 << show_bits);
    ClassicLUTInit();
  }

  if (mode == ModeLevels) {
    if (!vi.IsPlanar()) {
      env->ThrowError("Histogram: Levels mode only available in PLANAR.");
    }
    optionValid = option.IsFloat();
    const double factor = option.AsDblDef(100.0); // Population limit % factor
    if (std::isnan(factor) || factor < 0.0 || factor > 100.0) {
      env->ThrowError("Histogram: Levels population clamping must be between 0 and 100%");
    }
    // put diagram on the right side
    if (keepsource) {
      vi.width += (1 << show_bits); // 256 for 8 bit
      vi.height = max(256, vi.height);
    } else { // or keep it alone
      vi.width = (1 << show_bits);
      vi.height = 256; // only 224+1 (3*64 + 2*16 + 1) is used
    }
  }

  if (mode == ModeColor) {
    if (vi.IsRGB()) {
      env->ThrowError("Histogram: Color mode is not available in RGB.");
    }
    if (!vi.IsPlanar()) {
      env->ThrowError("Histogram: Color mode only available in PLANAR.");
    }
    if (vi.IsY()) {
      env->ThrowError("Histogram: Color mode not available in greyscale.");
    }
    // put diagram on the right side
    if (keepsource) {
      vi.width += (1 << show_bits); // 256 for 8 bit
      vi.height = max(1 << show_bits, vi.height);
    } else {
      vi.width = (1 << show_bits); // 256 for 8 bit
      vi.height = 1 << show_bits;
    }
  }

  if (mode == ModeColor2) {
    if (vi.IsRGB()) {
      env->ThrowError("Histogram: Color2 mode is not available in RGB.");
    }
    if (!vi.IsPlanar()) {
      env->ThrowError("Histogram: Color2 mode only available in PLANAR.");
    }
    if (vi.IsY()) {
      env->ThrowError("Histogram: Color2 mode not available in greyscale.");
    }

    // put circle on the right side
    if (keepsource) {
      vi.width += (1 << show_bits);                 // 256 for 8 bit
      vi.height = max((1 << show_bits), vi.height); // yes, height can change
    } else {
      vi.width = (1 << show_bits);  // 256 for 8 bit
      vi.height = (1 << show_bits); // yes, height can change
    }

    // precalculate 15 degree marker dots
    const int half = (1 << (show_bits - 1)) - 1; // 127
    // dots are placed somewhat inside to the colorful circle, which is thicker for higher show_bits
    color2_innerF = 124.9;                                           // .9 is for better visuals in subsampled mode
    int R = (int)(1 + color2_innerF * (1 << (show_bits - 8)) + 0.5); // 126 for 8 bits

    for (int y = 0; y < 24; y++) { // just inside the big circle
      deg15c[y] = (int)(R * cos(y * PI / 12.) + 0.5) + half;
      deg15s[y] = (int)(-R * sin(y * PI / 12.) + 0.5) + half;
    }
  }

  if (mode == ModeLuma && !vi.IsYUV() && !vi.IsYUVA()) {
    env->ThrowError("Histogram: Luma mode only available in YUV(A).");
  }

  if ((mode == ModeStereoY8) || (mode == ModeStereo) || (mode == ModeOverlay)) {

    child->SetCacheHints(CACHE_AUDIO, 4096 * 1024);

    if (!vi.HasVideo()) {
      mode = ModeStereo; // force mode to ModeStereo.
      vi.fps_numerator = 25;
      vi.fps_denominator = 1;
      vi.num_frames = vi.FramesFromAudioSamples(vi.num_audio_samples);
    }
    if (mode == ModeOverlay) {
      if (keepsource) {
        vi.height = max(512, vi.height);
        vi.width = max(512, vi.width);
      } else {
        vi.height = 512;
        vi.width = 512;
      }
      if (vi.IsRGB()) {
        env->ThrowError("Histogram: StereoOverlay mode is not available in RGB.");
      }
      if (!vi.IsPlanar()) {
        env->ThrowError("Histogram: StereoOverlay only available in Y or YUV(A).");
      }
    } else if (mode == ModeStereoY8) {
      vi.pixel_type = VideoInfo::CS_Y8;
      vi.height = 512;
      vi.width = 512;
    } else {
      vi.pixel_type = VideoInfo::CS_YV12;
      vi.height = 512;
      vi.width = 512;
    }
    if (!vi.HasAudio()) {
      env->ThrowError("Histogram: Stereo mode requires samples!");
    }
    if (vi.AudioChannels() != 2) {
      env->ThrowError("Histogram: Stereo mode only works on two audio channels.");
    }

    aud_clip = env->Invoke("ConvertAudioTo16bit", child).AsClip();
  }

  if (mode == ModeAudioLevels) {
    child->SetCacheHints(CACHE_AUDIO, 4096 * 1024);
    if (vi.IsRGB()) {
      env->ThrowError("Histogram: Audiolevels mode is not available in RGB.");
    }
    if (!vi.IsPlanar()) {
      env->ThrowError("Histogram: Audiolevels mode only available in planar YUV.");
    }
    if (vi.IsY8()) {
      env->ThrowError("Histogram: AudioLevels mode not available in Y8.");
    }
    const int minimum_width = (1 + vi.AudioChannels() * 2) * 4;
    if (vi.width < minimum_width)
      env->ThrowError("Histogram: AudioLevels width is too small for all audio bars");

    aud_clip = env->Invoke("ConvertAudioTo16bit", child).AsClip();
  }

  if (!optionValid && option.Defined())
    env->ThrowError("Histogram: Unknown optional value.");
}

PVideoFrame __stdcall Histogram::GetFrame(int n, IScriptEnvironment* env) {
  switch (mode) {
    case ModeClassic:
      return DrawModeClassic(n, env);
    case ModeLevels:
      return DrawModeLevels(n, env);
    case ModeColor:
      return DrawModeColor(n, env);
    case ModeColor2:
      return DrawModeColor2(n, env);
    case ModeLuma:
      return DrawModeLuma(n, env);
    case ModeStereoY8:
    case ModeStereo:
      return DrawModeStereo(n, env);
    case ModeOverlay:
      return DrawModeOverlay(n, env);
    case ModeAudioLevels:
      return DrawModeAudioLevels(n, env);
  }
  return DrawModeClassic(n, env);
}

AVSValue __cdecl Histogram::Create(AVSValue args, void*, IScriptEnvironment* env) {
  const char* st_m = args[1].AsString("classic");

  Mode mode = ModeClassic;

  if (!lstrcmpi(st_m, "classic"))
    mode = ModeClassic;

  if (!lstrcmpi(st_m, "levels"))
    mode = ModeLevels;

  if (!lstrcmpi(st_m, "color"))
    mode = ModeColor;

  if (!lstrcmpi(st_m, "color2"))
    mode = ModeColor2;

  if (!lstrcmpi(st_m, "luma"))
    mode = ModeLuma;

  if (!lstrcmpi(st_m, "stereoY8"))
    mode = ModeStereoY8;

  if (!lstrcmpi(st_m, "stereo"))
    mode = ModeStereo;

  if (!lstrcmpi(st_m, "stereooverlay"))
    mode = ModeOverlay;

  if (!lstrcmpi(st_m, "audiolevels"))
    mode = ModeAudioLevels;

  const VideoInfo& vi_orig = args[0].AsClip()->GetVideoInfo();

  if (mode == ModeLevels && vi_orig.IsRGB() && !vi_orig.IsPlanar()) {
    // as Levels can work for PlanarRGB, convert packed RGB to planar, then back
    // better that nothing
    AVSValue new_args[1] = {args[0].AsClip()};
    PClip clip;
    if (vi_orig.IsRGB24() || vi_orig.IsRGB48()) {
      clip = env->Invoke("ConvertToPlanarRGB", AVSValue(new_args, 1)).AsClip();
    } else if (vi_orig.IsRGB32() || vi_orig.IsRGB64()) {
      clip = env->Invoke("ConvertToPlanarRGBA", AVSValue(new_args, 1)).AsClip();
    }
    Histogram* Result =
        new Histogram(clip, mode, args[2], args[3].AsInt(8), args[4].AsBool(true), args[5].AsBool(true), env);

    AVSValue new_args2[1] = {Result};
    if (vi_orig.IsRGB24()) {
      return env->Invoke("ConvertToRGB24", AVSValue(new_args2, 1)).AsClip();
    } else if (vi_orig.IsRGB48()) {
      return env->Invoke("ConvertToRGB48", AVSValue(new_args2, 1)).AsClip();
    } else if (vi_orig.IsRGB32()) {
      return env->Invoke("ConvertToRGB32", AVSValue(new_args2, 1)).AsClip();
    } else { // if (vi_orig.IsRGB64())
      return env->Invoke("ConvertToRGB64", AVSValue(new_args2, 1)).AsClip();
    }
  } else {
    return new Histogram(args[0].AsClip(), mode, args[2], args[3].AsInt(8), args[4].AsBool(true), args[5].AsBool(true),
                         env);
  }
}

} // namespace aif::filters::histogram
