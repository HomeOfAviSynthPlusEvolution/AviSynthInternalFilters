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
  const double max = min(maxSat + p, 180.0);
  const double min = ::max(minSat - p, 0.0);

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

// for float
static bool ProcessPixelUnscaled(double X, double Y, double startHue, double endHue, double maxSat, double minSat,
                                 double p, double& dSat) {
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
  const double max = min(maxSat + p, 180.0);
  const double min = ::max(minSat - p, 0.0);

  // Outside of [min-p, max+p] no adjustment
  // minSat-p <= (U^2 + V^2) <= maxSat+p
  if (W <= min * min || max * max <= W)
    return false; // don't adjust

  // Interpolate saturation value
  const double holdSat = W < 180.0 * 180.0 ? sqrt(W) : 180.0;

  if (holdSat < minSat) { // within p of lower range
    dSat += ((1 - dSat) * (minSat - holdSat) / p);
  } else { // within p of upper range
    dSat += ((1 - dSat) * (holdSat - maxSat) / p);
  }

  return true;
}

/**********************
******   Tweak    *****
**********************/
template <typename pixel_t, bool bpp10_14, bool dither>
void Tweak::tweak_calc_luma(BYTE* srcp, int src_pitch, float minY, float maxY, int width, int height) {
  float ditherval = 0.0f;
  for (int y = 0; y < height; ++y) {
    const int _y = (y << 4) & 0xf0;
    for (int x = 0; x < width; ++x) {
      if (dither)
        ditherval = (ditherMap[(x & 0x0f) | _y] * dither_strength + bias_dither_luma) /
                    (float)scale_dither_luma; // 0x00..0xFF -> -0.7F .. + 0.7F (+/- maxrange/512)
      float y0 = reinterpret_cast<pixel_t*>(srcp)[x] - minY;
      if (bpp10_14)
        y0 = minY + (y0 + ditherval) * (float)dcont +
             (float)(1 << (bits_per_pixel - 8)) *
                 (float)dbright; // dbright parameter always 0..255. Scale to 0..255*4, 0.. 255*256
      else if (pixelsize == 2)
        y0 = minY + (y0 + ditherval) * (float)dcont +
             256.0f * (float)dbright; // dbright parameter always 0..255. Scale to 0..255*4, 0.. 255*256
      else if (pixelsize == 4)
        y0 = minY + (y0 + ditherval) * (float)dcont +
             (float)dbright / 256.0f; // dbright parameter always 0..255, scale it to 0..1
      else                            // pixelsize == 1
        y0 = minY + ((y0 + ditherval) * (float)dcont +
                     1.0f * (float)dbright); // dbright parameter always 0..255. Scale to 0..255*4, 0.. 255*256

      reinterpret_cast<pixel_t*>(srcp)[x] = (pixel_t)clamp(y0, minY, maxY);
      /*
      int y = int(((ii - range_low * scale_dither_luma)*_cont + _bright * scale_dither_luma + bias_dither_luma) / scale_dither_luma + range_low + 0.5); // 256 _cont & _bright param range
      // coring, dither:
      // int y = int(((ii - 16 * 256)*_cont + _bright * 256 - 127.5) / 256 + 16.5); // 256 _cont & _bright param range
      // coring, no dither:
      // int y = int(((ii - 16)*_cont + _bright) + 16.5); // 256 _cont & _bright param range
      // no coring, dither:
      // int y = int((ii *_cont + _bright * 256 - 127.5) / 256 + 0.5 ); // 256 _cont & _bright param range
      // no coring, no dither:
      // int y = int((ii *_cont + _bright) + 0.5 ); // 256 _cont & _bright param range
      */
    }
    srcp += src_pitch;
  }
}

template <typename pixel_t, bool dither>
void Tweak::tweak_calc_chroma(BYTE* srcpu, BYTE* srcpv, int src_pitch, int width, int height, float minUV,
                              float maxUV) {
  // no lookup, alway true for 16/32 bit, optional for 8 bit
  const double Hue = (dhue * PI) / 180.0;
  // 100% equals sat=119 (= maximal saturation of valid RGB (R=255,G=B=0)
  // 150% (=180) - 100% (=119) overshoot
  const double minSat = 1.19 * dminSat;
  const double maxSat = 1.19 * dmaxSat;

  const double p = dinterp * 1.19; // Same units as minSat/maxSat

  const int minUVi = (int)minUV;
  const int maxUVi = (int)maxUV;

  float ditherval = 0.0;
  float u, v;
  const float cosHue = (float)cos(Hue);
  const float sinHue = (float)sin(Hue);
  // no lut, realcalc, float internals
  const float pixel_range = sizeof(pixel_t) == 4 ? 1.0f : (float)(max_pixel_value + 1);

  double uv_range_corr = 255.0;

  const bool isFloat = sizeof(pixel_t) == 4;

  for (int y = 0; y < height; ++y) {
    const int _y = (y << 2) & 0xC;
    for (int x = 0; x < width; ++x) {
      if (dither)
        ditherval = ((float(ditherMap4[(x & 0x3) | _y]) * dither_strength + bias_dither_chroma) /
                     scale_dither_chroma); // +/-0.5 on 0..255 range
      pixel_t orig_u = reinterpret_cast<pixel_t*>(srcpu)[x];
      pixel_t orig_v = reinterpret_cast<pixel_t*>(srcpv)[x];
      u = isFloat ? (orig_u - limits.middle_chroma_f) : (orig_u - limits.middle_chroma);
      v = isFloat ? (orig_v - limits.middle_chroma_f) : (orig_v - limits.middle_chroma);

      u = (u + (dither ? ditherval : 0)) / (sizeof(pixel_t) == 4 ? 1.0f : pixel_range); // going from 0..1 to +/-0.5
      v = (v + (dither ? ditherval : 0)) / (sizeof(pixel_t) == 4 ? 1.0f : pixel_range);

      double dWorkSat = dsat; // init from original param
      if (allPixels ||
          ProcessPixelUnscaled(v * uv_range_corr, u * uv_range_corr, dstartHue, dendHue, maxSat, minSat, p, dWorkSat)) {
        float du = ((u * cosHue + v * sinHue) * (float)dWorkSat);
        float dv = ((v * cosHue - u * sinHue) * (float)dWorkSat);

        if (isFloat) {
          du = du + limits.middle_chroma_f;
          dv = dv + limits.middle_chroma_f;
        } else {
          // back to 0..1
          du = du + 0.5f;
          dv = dv + 0.5f;
        }

        if (isFloat) {
          reinterpret_cast<pixel_t*>(srcpu)[x] = (pixel_t)clamp(du, minUV, maxUV);
          reinterpret_cast<pixel_t*>(srcpv)[x] = (pixel_t)clamp(dv, minUV, maxUV);
        } else {
          reinterpret_cast<pixel_t*>(srcpu)[x] = (pixel_t)clamp((int)(du * pixel_range), minUVi, maxUVi);
          reinterpret_cast<pixel_t*>(srcpv)[x] = (pixel_t)clamp((int)(dv * pixel_range), minUVi, maxUVi);
        }
      } else {
        if (isFloat) {
          reinterpret_cast<pixel_t*>(srcpu)[x] = (pixel_t)clamp((float)orig_u, minUV, maxUV);
          reinterpret_cast<pixel_t*>(srcpv)[x] = (pixel_t)clamp((float)orig_v, minUV, maxUV);
        } else {
          reinterpret_cast<pixel_t*>(srcpu)[x] = (pixel_t)clamp((int)(orig_u), minUVi, maxUVi);
          reinterpret_cast<pixel_t*>(srcpv)[x] = (pixel_t)clamp((int)(orig_v), minUVi, maxUVi);
        }
      }
    }
    srcpu += src_pitch;
    srcpv += src_pitch;
  }
}
