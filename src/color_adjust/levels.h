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
#include <cstdint>
#include "kernel/types.h"
class Levels : public GenericVideoFilter
/**
  * Class for adjusting levels in a clip
 **/
{
  const uint32_t cpu_mask_;

public:
  Levels(PClip _child, float _in_min, double _gamma, float _in_max, float _out_min, float _out_max, bool _coring,
         bool _dither, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    (void)frame_range;
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env);

private:
  BYTE *map, *mapchroma;
  bool need_chroma;
  bool coring;
  bool dither;
  double gamma;
  bool use_gamma;
  float in_min_f, in_max_f, out_min_f, out_max_f;

  // avs+
  int pixelsize;
  int bits_per_pixel; // 8,10..16
  bool use_lut;

  int real_lookup_size;

  luma_chroma_limits_t limits;

  float divisor_f;
  float out_diff_f; // precalc for speed

  float ditherMap_f[256];

  float bias_dither;
  float dither_strength;

  template <bool chroma, bool use_gamma>
  AVS_FORCEINLINE float calcPixel(const float pixel);
};
