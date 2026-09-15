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

// Avisynth filter: Layer
// by "poptones" (poptones@myrealbox.com)

#include "mask_hs.h"
#include <cmath>
#include <algorithm>
namespace aif::filters::mask {
#include "kernel/hue_selection.h"
static void __cdecl free_buffer(void* p, IScriptEnvironment* e) {
  e->Free(p);
}
MaskHS::MaskHS(PClip _child, double _startHue, double _endHue, double _maxSat, double _minSat, bool _coring,
               bool _realcalc, IScriptEnvironment* env)
    : GenericVideoFilter(_child), dstartHue(_startHue), dendHue(_endHue), dmaxSat(_maxSat), dminSat(_minSat),
      coring(_coring), realcalc(_realcalc) {
  if (!std::isfinite(dstartHue) || !std::isfinite(dendHue) || !std::isfinite(dminSat) || !std::isfinite(dmaxSat))
    env->ThrowError("MaskHS: hue and saturation must be finite");
  if (vi.IsRGB())
    env->ThrowError("MaskHS: YUV data only (no RGB)");

  if (vi.NumComponents() == 1) {
    env->ThrowError("MaskHS: clip must contain chroma.");
  }

  if (dstartHue < 0.0 || dstartHue >= 360.0)
    env->ThrowError("MaskHS: startHue must be greater than or equal to 0.0 and less than 360.0");

  if (dendHue <= 0.0 || dendHue > 360.0)
    env->ThrowError("MaskHS: endHue must be greater than 0.0 and less than or equal to 360.0");

  if (dminSat >= dmaxSat)
    env->ThrowError("MaskHS: MinSat must be less than MaxSat");

  if (dminSat < 0.0 || dminSat >= 150.0)
    env->ThrowError("MaskHS: minSat must be greater than or equal to 0 and less than 150.");

  if (dmaxSat <= 0.0 || dmaxSat > 150.0)
    env->ThrowError("MaskHS: maxSat must be greater than 0 and less than or equal to 150.");

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();
  lut_size = pixelsize == 4 ? 0 : 1 << bits_per_pixel;
  max_pixel_value = pixelsize == 4 ? 1 : lut_size - 1;

  get_limits(limits, bits_per_pixel);

  mask_low = coring ? limits.tv_range_low : 0;
  mask_high = coring ? limits.tv_range_hi_luma : max_pixel_value;

  if (bits_per_pixel == 32) {
    mask_low_f = coring ? limits.tv_range_low_luma_f : limits.full_range_low_luma_f;
    mask_high_f = coring ? limits.tv_range_hi_luma_f : limits.full_range_hi_luma_f;
  }

  realcalc_chroma = realcalc;
  if (vi.IsPlanar() && (bits_per_pixel > 12)) // max bitdepth is 12 for lut
    realcalc_chroma = true;

  // 100% equals sat=119 (= maximal saturation of valid RGB (R=255,G=B=0)
  // 150% (=180) - 100% (=119) overshoot
  minSat = 1.19 * dminSat;
  maxSat = 1.19 * dmaxSat;

  if (!(realcalc_chroma && vi.IsPlanar())) { // fill lookup tables for UV
    size_t map_size = pixelsize * lut_size * lut_size;
    // for  8 bit : 1 * 256 * 256 = 65536 byte
    // for 10 bit : 2 * 1024 * 1024 = 2 MByte
    // for 12 bit : 2 * 4096 * 4096 = 32 MByte
    mapUV = static_cast<uint8_t*>(env->Allocate(
        map_size, 8, AVS_NORMAL_ALLOC)); // uint16_t for (U+V bytes), casted to uint32_t for (U+V words in non-8 bit)
    if (!mapUV)
      env->ThrowError("Tweak: Could not reserve memory.");
    env->AtExit(free_buffer, mapUV);

    // apply mask
    double uv_range_corr = 1.0 / (1 << (bits_per_pixel - 8)); // no float here
    for (int u = 0; u < lut_size; u++) {
      const double destu =
          (u - limits.middle_chroma) * uv_range_corr; // processpixel's minSat and maxSat is for 256 range
      int ushift = u << bits_per_pixel;
      for (int v = 0; v < lut_size; v++) {
        const double destv = (v - limits.middle_chroma) * uv_range_corr;
        int iSat = 0; // won't be used in MaskHS; interpolation is skipped since p==0:
        bool ppres = ProcessPixel(destv, destu, dstartHue, dendHue, maxSat, minSat, 0.0, iSat);
        if (pixelsize == 1)
          mapUV[ushift | v] = ppres ? mask_high : mask_low;
        else
          reinterpret_cast<uint16_t*>(mapUV)[ushift | v] = ppres ? mask_high : mask_low;
      }
    }
  } // end of lut calculation
  // #define MaskPointResizing
#ifndef MaskPointResizing
  vi.width >>= vi.GetPlaneWidthSubsampling(PLANAR_U);
  vi.height >>= vi.GetPlaneHeightSubsampling(PLANAR_U);
#endif
  switch (bits_per_pixel) {
    case 8:
      vi.pixel_type = VideoInfo::CS_Y8;
      break;
    case 10:
      vi.pixel_type = VideoInfo::CS_Y10;
      break;
    case 12:
      vi.pixel_type = VideoInfo::CS_Y12;
      break;
    case 14:
      vi.pixel_type = VideoInfo::CS_Y14;
      break;
    case 16:
      vi.pixel_type = VideoInfo::CS_Y16;
      break;
    case 32:
      vi.pixel_type = VideoInfo::CS_Y32;
      break;
  }
}

PVideoFrame __stdcall MaskHS::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrameP(vi, &src);

  uint8_t* dstp = dst->GetWritePtr();
  int dst_pitch = dst->GetPitch();

#include "kernel/hue_mask.inc"
  return dst;
}

AVSValue __cdecl MaskHS::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new MaskHS(args[0].AsClip(),
                    args[1].AsFloat(0.0),   // startHue
                    args[2].AsFloat(360.0), // endHue
                    args[3].AsFloat(150.0), // maxSat
                    args[4].AsFloat(0.0),   // minSat
                    args[5].AsBool(false),  // coring
                    args[6].AsBool(false),  // realcalc
                    env);
}

} // namespace aif::filters::mask
