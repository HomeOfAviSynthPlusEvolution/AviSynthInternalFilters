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

#pragma once
#define PI 3.141592653589793
// Limits for MaskHS
static void get_limits(luma_chroma_limits_t& d, int bits_per_pixel) {
  int tv_range_lo_luma_8 = 16;
  int tv_range_hi_luma_8 = 235;
  int tv_range_lo_chroma_8 = tv_range_lo_luma_8;
  int tv_range_hi_chroma_8 = 240;

  if (bits_per_pixel == 32) {
    d.tv_range_low_luma_f = tv_range_lo_luma_8 / 255.0f;
    d.tv_range_hi_luma_f = tv_range_hi_luma_8 / 255.0f;
    d.full_range_low_luma_f = 0.0f;
    d.full_range_hi_luma_f = 1.0f;
#ifdef FLOAT_CHROMA_IS_HALF_CENTERED
    d.middle_chroma_f = 0.5f;
#else
    d.middle_chroma_f = 0.0f;
#endif
    d.tv_range_low_chroma_f = (tv_range_lo_chroma_8 - 128) / 255.0f + d.middle_chroma_f; // -112
    d.tv_range_hi_chroma_f = (tv_range_hi_chroma_8 - 128) / 255.0f + d.middle_chroma_f;  // 112
    d.full_range_low_chroma_f = d.middle_chroma_f - 0.5f;                                // -0.5..0.5 or 0..1.0
    d.full_range_hi_chroma_f = d.middle_chroma_f + 0.5f;

    d.range_luma_f = d.tv_range_hi_luma_f - d.tv_range_low_luma_f;
    d.range_chroma_f = d.tv_range_hi_chroma_f - d.tv_range_low_chroma_f;
  } else {
    d.tv_range_low = tv_range_lo_luma_8 << (bits_per_pixel - 8); // 16-240,64-960, 256-3852,... 4096-61692
    d.tv_range_hi_luma = tv_range_hi_luma_8 << (bits_per_pixel - 8);
    d.tv_range_hi_chroma = tv_range_hi_chroma_8 << (bits_per_pixel - 8);
    d.middle_chroma = 1 << (bits_per_pixel - 1);            // 128
    d.range_luma = d.tv_range_hi_luma - d.tv_range_low;     // 219
    d.range_chroma = d.tv_range_hi_chroma - d.tv_range_low; // 224
  }
}

/********************************
 *******   MaskHS Filter   ******
 ********************************/

/* Hue and saturation selection for MaskHS. */
static bool ProcessPixel(double X, double Y, double startHue, double endHue, double maxSat, double minSat, double p,
                         int& iSat) {
  // a hue analog
  double T = atan2(X, Y) * 180.0 / PI;
  if (T < 0.0)
    T += 360.0;

  // startHue <= hue <= endHue
  if (startHue < endHue) {
    if (T > endHue || T < startHue)
      return false;
  } else {
    if (T < startHue && T > endHue)
      return false;
  }

  const double W = X * X + Y * Y;

  // In Range, full adjust but no need to interpolate
  if (minSat * minSat <= W && W <= maxSat * maxSat)
    return true;

  // p == 0 (no interpolation) needed for MaskHS
  if (p == 0.0)
    return false;

  // Interpolation range is +/-p for p>0
  // 180 is not in degrees!
  // its sqrt(127^2 + 127^2): max overshoot for 8 bits; U and V is 0 +/- 127
  const double max = std::min(maxSat + p, 180.0);
  const double min = std::max(minSat - p, 0.0);

  // Outside of [min-p, max+p] no adjustment
  // minSat-p <= (U^2 + V^2) <= maxSat+p
  if (W <= min * min || max * max <= W)
    return false; // don't adjust

  // Interpolate saturation value
  const double holdSat = W < 180.0 * 180.0 ? sqrt(W) : 180.0;

  if (holdSat < minSat) { // within p of lower range
    iSat += (int)((512 - iSat) * (minSat - holdSat) / p);
  } else { // within p of upper range
    iSat += (int)((512 - iSat) * (holdSat - maxSat) / p);
  }

  return true;
}
