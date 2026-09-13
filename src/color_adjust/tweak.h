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

#pragma once
#include <avisynth.h>
#include "kernel/types.h"
class Tweak : public GenericVideoFilter {
public:
  Tweak(PClip _child, double _hue, double _sat, double _bright, double _cont, bool _coring, double _startHue,
        double _endHue, double _maxSat, double _minSat, double _interp, bool _dither, bool _realcalc,
        double _dither_strength, IScriptEnvironment* env);

  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    (void)frame_range;
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl Create(AVSValue args, void* user_data, IScriptEnvironment* env);

private:
  template <typename pixel_t, bool bpp10_14, bool dither>
  void tweak_calc_luma(BYTE* srcp, int src_pitch, float minY, float maxY, int width, int height);

  template <typename pixel_t, bool dither>
  void tweak_calc_chroma(BYTE* srcpU, BYTE* srcpV, int src_pitch, int width, int height, float minUV, float maxUV);

  int Sin, Cos;
  int Sat, Bright, Cont;
  bool coring, dither;

  const bool realcalc; // no lookup, realtime calculation, always for 16/32 bits
  double dhue, dsat, dbright, dcont, dstartHue, dendHue, dmaxSat, dminSat, dinterp;

  bool allPixels;

  BYTE* map;
  uint16_t* mapUV;
  // avs+
  bool realcalc_luma;
  bool realcalc_chroma;

  int pixelsize;
  int bits_per_pixel; // 8,10..16
  int max_pixel_value;
  int lut_size;

  luma_chroma_limits_t limits;

  int scale_dither_luma;
  int divisor_dither_luma;
  float bias_dither_luma;

  int scale_dither_chroma;
  int divisor_dither_chroma;
  float bias_dither_chroma;

  float dither_strength;
};
