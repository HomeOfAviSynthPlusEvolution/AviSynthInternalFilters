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

#include "tweak.h"
#include "kernel/common.h"
#include "kernel_adapter.h"
#include "kernel/tweak.h"
Tweak::Tweak(PClip _child, double _hue, double _sat, double _bright, double _cont, bool _coring, double _startHue,
             double _endHue, double _maxSat, double _minSat, double p, bool _dither, bool _realcalc,
             double _dither_strength, IScriptEnvironment* env)
    : GenericVideoFilter(_child), coring(_coring), dither(_dither), realcalc(_realcalc), dhue(_hue), dsat(_sat),
      dbright(_bright), dcont(_cont), dstartHue(_startHue), dendHue(_endHue), dmaxSat(_maxSat), dminSat(_minSat),
      dinterp(p), dither_strength((float)_dither_strength) {
  if (vi.IsRGB())
    env->ThrowError("Tweak: YUV data only (no RGB)");

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();
  lut_size = pixelsize == 4 ? 0 : 1 << bits_per_pixel;
  max_pixel_value = pixelsize == 4 ? 1 : lut_size - 1;
  int safe_luma_lookup_size =
      (pixelsize == 1) ? 256 : 65536; // avoids lut overflow in case of non-standard content of a 10 bit clip

  get_limits(limits, bits_per_pixel); // tv range limits

  scale_dither_luma = 1;
  divisor_dither_luma = 1;
  bias_dither_luma = 0.0;

  scale_dither_chroma = 1;
  divisor_dither_chroma = 1;
  bias_dither_chroma = 0.0;

  if (pixelsize == 4)
    dither_strength /= 65536.0f; // same dither range as for a 16 bit clip
  // Set dither_strength = 4.0 for 10 bits or 256.0 for 16 bits in order to have same dither range as for 8 bits
  // Otherwise dithering is always +/- 0.5 at all bit-depth

  if (dither) {
    // lut scale settings
    scale_dither_luma = 256; // lower 256 is dither value
    divisor_dither_luma *= 256;
    bias_dither_luma = pixelsize == 4 ? -127.5f * dither_strength : -(256.0f * dither_strength - 1) / 2;
    // original bias: -127.5 or -(256.0f * dither_strength - 1) / 2;
    // dither strength =1 = (1 << (8-8))
    // dither min: int( (0*1-127.5)/256+0.5) = -0.498046875 + 0.5 = 0,001953125
    // dither max: int( (255*1-127.5)/256+0.5) = 0,998046875

    // 16 bit: 32767,5
    // dither strength =256 = (1 << (16-8))
    // dither min: int( (0*256-32767,5)/256+0.5)   = -127,498046875
    // dither max: int( (255*256-32767,5)/256+0.5) = 127,501953125

    scale_dither_chroma = 16; // lower 16 is dither value
    divisor_dither_chroma *= 16;
    // Float dither must remain centered on zero at every strength.
    bias_dither_chroma = pixelsize == 4 ? -7.5f * dither_strength : -(16.0f * dither_strength - 1) / 2;
  }

  // Flag to skip special processing if doing all pixels
  // If defaults, don't check for ranges, just do all
  allPixels = (_startHue == 0.0 && _endHue == 360.0 && _maxSat == 150.0 && _minSat == 0.0);

  if (vi.NumComponents() == 1) {
    if (!(_hue == 0.0 && _sat == 1.0 && allPixels))
      env->ThrowError("Tweak: bright and cont are the only options available for greyscale.");
  }

  if (_startHue < 0.0 || _startHue >= 360.0)
    env->ThrowError("Tweak: startHue must be greater than or equal to 0.0 and less than 360.0");

  if (_endHue <= 0.0 || _endHue > 360.0)
    env->ThrowError("Tweak: endHue must be greater than 0.0 and less than or equal to 360.0");

  if (_minSat >= _maxSat)
    env->ThrowError("Tweak: MinSat must be less than MaxSat");

  if (_minSat < 0.0 || _minSat >= 150.0)
    env->ThrowError("Tweak: minSat must be greater than or equal to 0 and less than 150.");

  if (_maxSat <= 0.0 || _maxSat > 150.0)
    env->ThrowError("Tweak: maxSat must be greater than 0 and less than or equal to 150.");

  if (p >= 150.0 || p < 0.0)
    env->ThrowError("Tweak: Interp must be greater than or equal to 0 and less than 150.");

  Sat = (int)(_sat * 512); // 9 bits extra precision
  Cont = (int)(_cont * 512);
  Bright = (int)_bright;

  const double Hue = (_hue * PI) / 180.0;
  const double SIN = sin(Hue);
  const double COS = cos(Hue);

  Sin = (int)(SIN * 4096 + 0.5);
  Cos = (int)(COS * 4096 + 0.5);

  realcalc_luma = realcalc; // from parameter
  realcalc_chroma = realcalc;
  if (vi.IsPlanar() && (bits_per_pixel > 10))
    realcalc_chroma = true;
  if (vi.IsPlanar() && (bits_per_pixel == 32))
    realcalc_luma = true;
  // 8/10bit: chroma lut OK. 12+ bits: force no lookup tables.
  // 8-16bit: luma lut OK. float: force no lookup tables.

  // fill brightness/constrast lookup tables
  if (!(realcalc_luma && vi.IsPlanar())) {
    size_t map_size = pixelsize * safe_luma_lookup_size * scale_dither_luma;
    // for 10-16 bit with dither: 2 * 65536 * 256 = 33 MByte
    //               w/o  dither: 2 * 65536 = 128 KByte
    map = static_cast<uint8_t*>(env->Allocate(map_size, 8, AVS_NORMAL_ALLOC));
    if (!map)
      env->ThrowError("Tweak: Could not reserve memory.");
    env->AtExit(free_buffer, map);

    if (bits_per_pixel > 8 && bits_per_pixel < 16) // make lut table safe for 10-14 bit garbage
      std::fill_n((uint16_t*)map, map_size / pixelsize, max_pixel_value);

    int range_low = coring ? limits.tv_range_low : 0;
    int range_high = coring ? limits.tv_range_hi_luma : max_pixel_value;

    // dither_scale_luma = 1 if no dither, 256 if dither
    /* create luma lut for brightness and contrast */
    for (int i = 0; i < lut_size * scale_dither_luma; i++) {
      int ii;
      if (dither) {
        ii = (i & 0xFFFFFF00) + (int)((i & 0xFF) * dither_strength);
      } else {
        ii = i;
      }
      // _bright param range is accepted as 0..256
      int y = (int)(((ii - range_low * scale_dither_luma) * _cont +
                     _bright * (1 << (bits_per_pixel - 8)) * scale_dither_luma + bias_dither_luma) /
                        scale_dither_luma +
                    range_low + 0.5);

      // coring, dither:
      // int y = int(((ii - 16 * 256)*_cont + _bright * 256 - 127.5) / 256 + 16.5); // 256 _cont & _bright param range
      // coring, no dither:
      // int y = int(((ii - 16)*_cont + _bright) + 16.5); // 256 _cont & _bright param range
      // no coring, dither:
      // int y = int((ii *_cont + _bright * 256 - 127.5) / 256 + 0.5 ); // 256 _cont & _bright param range
      // no coring, no dither:
      // int y = int((ii *_cont + _bright) + 0.5 ); // 256 _cont & _bright param range
      if (pixelsize == 1)
        map[i] = (BYTE)clamp(y, range_low, range_high);
      else
        reinterpret_cast<uint16_t*>(map)[i] = (uint16_t)clamp(y, range_low, range_high);
    }
  }
  // 100% equals sat=119 (= maximal saturation of valid RGB (R=255,G=B=0)
  // 150% (=180) - 100% (=119) overshoot
  const double minSat = 1.19 * _minSat;
  const double maxSat = 1.19 * _maxSat;

  p *= 1.19; // Same units as minSat/maxSat

  if (!(realcalc_chroma && vi.IsPlanar())) { // fill lookup tables for UV
    size_t map_size = pixelsize * lut_size * lut_size * 2 * scale_dither_chroma;
    // for 10 bit with dither: 2 * 1024 * 1024 * 2 * 4 = 4*64 MByte = 256M huh!
    // for 10 bit w/o  dither: 2 * 1024 * 1024 * 2 = 64 MByte

    mapUV = static_cast<uint16_t*>(env->Allocate(
        map_size, 8, AVS_NORMAL_ALLOC)); // uint16_t for (U+V bytes), casted to uint32_t for (U+V words in non-8 bit)
    if (!mapUV)
      env->ThrowError("Tweak: Could not reserve memory.");
    env->AtExit(free_buffer, mapUV);

    int range_low = coring ? limits.tv_range_low : 0;
    int range_high = coring ? limits.tv_range_hi_chroma : max_pixel_value;

    double uv_range_corr = 1.0 / (1 << (bits_per_pixel - 8));

    if (dither) {
      // lut chroma, dither
      for (int d = 0; d < scale_dither_chroma; d++) { // scale = 4    0..15 mini-dither
        for (int u = 0; u < lut_size; u++) {
          // dither_strength: optional correction for 8+ bit to have the same dither range as in 8 bits
          const double destu = (((u << 4) + d * dither_strength) + bias_dither_chroma) / scale_dither_chroma -
                               limits.middle_chroma; // scale_dither_chroma: 16
          for (int v = 0; v < lut_size; v++) {
            const double destv =
                (((v << 4) + d * dither_strength) + bias_dither_chroma) / scale_dither_chroma - limits.middle_chroma;
            int iSat = Sat;
            if (allPixels || ProcessPixel(destv * uv_range_corr, destu * uv_range_corr, _startHue, _endHue, maxSat,
                                          minSat, p, iSat)) {
              int du =
                  (int)((destu * COS + destv * SIN) * iSat + 0x100) >> 9; // back from the extra 9 bits Sat precision
              int dv = (int)((destv * COS - destu * SIN) * iSat + 0x100) >> 9;
              du = clamp(du + limits.middle_chroma, range_low, range_high);
              dv = clamp(dv + limits.middle_chroma, range_low, range_high);
              if (pixelsize == 1)
                mapUV[(u << 12) | (v << 4) | d] = (uint16_t)(du | (dv << 8)); // U and V: two bytes
              else
                reinterpret_cast<uint32_t*>(mapUV)[(u << (4 + bits_per_pixel)) | (v << 4) | d] =
                    (uint32_t)(du | (dv << 16)); // U and V: two words
            } else {
              if (pixelsize == 1)
                mapUV[(u << 12) | (v << 4) | d] =
                    (uint16_t)(clamp(u, range_low, range_high) |
                               (clamp(v, range_low, range_high) << 8)); // U and V: two bytes
              else
                reinterpret_cast<uint32_t*>(mapUV)[(u << (4 + bits_per_pixel)) | (v << 4) | d] =
                    (uint32_t)(clamp(u, range_low, range_high) |
                               ((clamp(v, range_low, range_high) << 16))); // U and V: two words
            }
          }
        }
      }
    } else {
      // lut chroma, no dither
      for (int u = 0; u < lut_size; u++) {
        const double destu = u - limits.middle_chroma;
        for (int v = 0; v < lut_size; v++) {
          const double destv = v - limits.middle_chroma;
          int iSat = Sat;
          if (allPixels ||
              ProcessPixel(destv * uv_range_corr, destu * uv_range_corr, _startHue, _endHue, maxSat, minSat, p, iSat)) {
            int du = int((destu * COS + destv * SIN) * iSat) >> 9; // back from the extra 9 bits Sat precision
            int dv = int((destv * COS - destu * SIN) * iSat) >> 9;
            du = clamp(du + limits.middle_chroma, range_low, range_high);
            dv = clamp(dv + limits.middle_chroma, range_low, range_high);
            if (pixelsize == 1)
              mapUV[(u << 8) | v] = (uint16_t)(du | (dv << 8)); // U and V: two bytes
            else
              reinterpret_cast<uint32_t*>(mapUV)[(u << bits_per_pixel) | v] =
                  (uint32_t)(du | (dv << 16)); // U and V: two words
          } else {
            if (pixelsize == 1)
              mapUV[(u << 8) | v] = (uint16_t)(clamp(u, range_low, range_high) |
                                               (clamp(v, range_low, range_high) << 8)); // U and V: two bytes
            else
              reinterpret_cast<uint32_t*>(mapUV)[(u << bits_per_pixel) | v] =
                  (uint32_t)(clamp(u, range_low, range_high) |
                             (clamp(v, range_low, range_high) << 16)); // U and V: two words
          }
        }
      }
    }
  }
}

PVideoFrame __stdcall Tweak::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  env->MakeWritable(&src);

  BYTE* srcp = src->GetWritePtr();

  int src_pitch = src->GetPitch();
  int height = src->GetHeight();
  int row_size = src->GetRowSize();

  if (vi.IsYUY2()) {

    if (dither) {
      const int UVwidth = vi.width / 2;
      for (int y = 0; y < height; y++) {
        {
          const int _y = (y << 4) & 0xf0;
          for (int x = 0; x < vi.width; ++x) {
            /* brightness and contrast */
            srcp[x * 2] = map[srcp[x * 2] << 8 | ditherMap[(x & 0x0f) | _y]];
          }
        }
        {
          const int _y = (y << 2) & 0xC;
          for (int x = 0; x < UVwidth; ++x) {
            const int _dither = ditherMap4[(x & 0x3) | _y];
            /* hue and saturation */
            const int u = srcp[x * 4 + 1];
            const int v = srcp[x * 4 + 3];
            const int mapped = mapUV[(u << 12) | (v << 4) | _dither];
            srcp[x * 4 + 1] = (BYTE)(mapped & 0xff);
            srcp[x * 4 + 3] = (BYTE)(mapped >> 8);
          }
        }
        srcp += src_pitch;
      }
    } else {
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < row_size; x += 4) {
          /* brightness and contrast */
          srcp[x] = map[srcp[x]];
          srcp[x + 2] = map[srcp[x + 2]];

          /* hue and saturation */
          const int u = srcp[x + 1];
          const int v = srcp[x + 3];
          const int mapped = mapUV[(u << 8) | v];
          srcp[x + 1] = (BYTE)(mapped & 0xff);
          srcp[x + 3] = (BYTE)(mapped >> 8);
        }
        srcp += src_pitch;
      }
    }
    // YUY2 end
  } else if (vi.IsPlanar()) {
    // brightness and contrast
    // no_lut and lut
    int width = row_size / pixelsize;
    if (realcalc_luma) {
      // no luma lookup! alway true for 32 bit, optional for 8-16 bits
      float maxY;
      float minY;
      // unique for each bit-depth, difference in the innermost loop (speed)
      maxY = (float)(coring ? limits.tv_range_hi_luma : max_pixel_value);
      minY = (float)(coring ? limits.tv_range_low : 0);

      if (pixelsize == 1) {
        if (dither)
          tweak_calc_luma<uint8_t, false, true>(srcp, src_pitch, minY, maxY, width, height);
        else
          tweak_calc_luma<uint8_t, false, false>(srcp, src_pitch, minY, maxY, width, height);
      } else if (bits_per_pixel < 16) {
        if (dither)
          tweak_calc_luma<uint16_t, true, true>(srcp, src_pitch, minY, maxY, width, height);
        else
          tweak_calc_luma<uint16_t, true, false>(srcp, src_pitch, minY, maxY, width, height);
      } else if (bits_per_pixel == 16) {
        if (dither)
          tweak_calc_luma<uint16_t, false, true>(srcp, src_pitch, minY, maxY, width, height);
        else
          tweak_calc_luma<uint16_t, false, false>(srcp, src_pitch, minY, maxY, width, height);
      } else {                               // float
        maxY = coring ? 235.0f / 256 : 1.0f; // scale into 0..1 range
        minY = coring ? 16.0f / 256 : 0;
        if (dither)
          tweak_calc_luma<float, false, true>(srcp, src_pitch, minY, maxY, width, height);
        else
          tweak_calc_luma<float, false, false>(srcp, src_pitch, minY, maxY, width, height);
      }
    } else {
      /* brightness and contrast */
      // use luma lookup for 8-16 bits
      if (dither) {
        if (pixelsize == 1) {
          for (int y = 0; y < height; ++y) {
            const int _y = (y << 4) & 0xf0;
            for (int x = 0; x < width; ++x) {
              /* brightness and contrast */
              srcp[x] = map[srcp[x] << 8 | ditherMap[(x & 0x0f) | _y]];
            }
            srcp += src_pitch;
          }
        } else { // pixelsize == 2
          for (int y = 0; y < height; ++y) {
            const int _y = (y << 4) & 0xf0;
            for (int x = 0; x < width; ++x) {
              reinterpret_cast<uint16_t*>(srcp)[x] = reinterpret_cast<uint16_t*>(
                  map)[reinterpret_cast<uint16_t*>(srcp)[x] << 8 | ditherMap[(x & 0x0f) | _y]];
              // no clamp, map is safely sized
            }
            srcp += src_pitch;
          }
        }
      } else {
        map_channel(srcp, src_pitch, srcp, src_pitch, width, height, map, pixelsize == 1 ? 8 : 16, 1, env);
      }
    }
    // Y: brightness and contrast done

    // UV: hue and saturation start
    src_pitch = src->GetPitch(PLANAR_U);
    BYTE* srcpu = src->GetWritePtr(PLANAR_U);
    BYTE* srcpv = src->GetWritePtr(PLANAR_V);
    row_size = src->GetRowSize(PLANAR_U);
    height = src->GetHeight(PLANAR_U);
    width = row_size / pixelsize;

    if (realcalc_chroma) {
      // no lookup, alway true for > 10 bit, optional for 8/10 bit
      float maxUV = (float)(coring ? limits.tv_range_hi_chroma : max_pixel_value);
      float minUV = (float)(coring ? limits.tv_range_low : 0);
      if (pixelsize == 1) {
        if (dither)
          tweak_calc_chroma<uint8_t, true>(srcpu, srcpv, src_pitch, width, height, minUV, maxUV);
        else
          tweak_calc_chroma<uint8_t, false>(srcpu, srcpv, src_pitch, width, height, minUV, maxUV);
      } else if (pixelsize == 2) {
        if (dither)
          tweak_calc_chroma<uint16_t, true>(srcpu, srcpv, src_pitch, width, height, minUV, maxUV);
        else
          tweak_calc_chroma<uint16_t, false>(srcpu, srcpv, src_pitch, width, height, minUV, maxUV);
      } else { // pixelsize == 4
        maxUV = coring ? limits.tv_range_hi_chroma_f : limits.full_range_hi_chroma_f;
        minUV = coring ? limits.tv_range_low_chroma_f : limits.full_range_low_chroma_f;
        if (dither)
          tweak_calc_chroma<float, true>(srcpu, srcpv, src_pitch, width, height, minUV, maxUV);
        else
          tweak_calc_chroma<float, false>(srcpu, srcpv, src_pitch, width, height, minUV, maxUV);
      }
    } else { // lookup UV
      if (dither) {
        // lut + dither
        if (pixelsize == 1) {
          for (int y = 0; y < height; ++y) {
            const int _y = (y << 2) & 0xC;
            for (int x = 0; x < width; ++x) {
              const int _dither = ditherMap4[(x & 0x3) | _y];
              /* hue and saturation */
              const int u = srcpu[x];
              const int v = srcpv[x];
              const int mapped = mapUV[(u << 12) | (v << 4) | _dither];
              srcpu[x] = (BYTE)(mapped & 0xff);
              srcpv[x] = (BYTE)(mapped >> 8);
            }
            srcpu += src_pitch;
            srcpv += src_pitch;
          }
        } else { // pixelsize == 2
          for (int y = 0; y < height; ++y) {
            const int _y = (y << 2) & 0xC;
            for (int x = 0; x < width; ++x) {
              const int _dither = ditherMap4[(x & 0x3) | _y]; // 0..15
              /* hue and saturation */
              const int u = clamp(0, (int)reinterpret_cast<uint16_t*>(srcpu)[x], max_pixel_value);
              const int v = clamp(0, (int)reinterpret_cast<uint16_t*>(srcpv)[x], max_pixel_value);
              const unsigned int mapped =
                  reinterpret_cast<uint32_t*>(mapUV)[(u << (4 + bits_per_pixel)) | (v << 4) | _dither];
              reinterpret_cast<uint16_t*>(srcpu)[x] = (uint16_t)(mapped & 0xffff);
              reinterpret_cast<uint16_t*>(srcpv)[x] = (uint16_t)(mapped >> 16);
            }
            srcpu += src_pitch;
            srcpv += src_pitch;
          }
        }
      } else {
        // lut + no dither
        if (pixelsize == 1) {
          for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
              /* hue and saturation */
              const int u = srcpu[x];
              const int v = srcpv[x];
              const int mapped = mapUV[(u << 8) | v];
              srcpu[x] = (BYTE)(mapped & 0xff);
              srcpv[x] = (BYTE)(mapped >> 8);
            }
            srcpu += src_pitch;
            srcpv += src_pitch;
          }
        } else { // pixelsize == 2
          for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
              const int u = clamp(0, (int)reinterpret_cast<uint16_t*>(srcpu)[x], max_pixel_value);
              const int v = clamp(0, (int)reinterpret_cast<uint16_t*>(srcpv)[x], max_pixel_value);
              const unsigned int mapped = reinterpret_cast<uint32_t*>(mapUV)[(u << bits_per_pixel) | v];
              reinterpret_cast<uint16_t*>(srcpu)[x] = (uint16_t)(mapped & 0xffff);
              reinterpret_cast<uint16_t*>(srcpv)[x] = (uint16_t)(mapped >> 16);
            }
            srcpu += src_pitch;
            srcpv += src_pitch;
          }
        }
      }
    }
  }

  return src;
}

AVSValue __cdecl Tweak::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new Tweak(args[0].AsClip(),
                   args[1].AsDblDef(0.0), // hue
                   args[2].AsDblDef(1.0), // sat
                   args[3].AsDblDef(0.0), // bright
                   args[4].AsDblDef(1.0), // cont
                   args[5].AsBool(true),  // coring
                   // not used even on intel. // args[6].AsBool(false),     // sse
                   args[7].AsDblDef(0.0),          // startHue
                   args[8].AsDblDef(360.0),        // endHue
                   args[9].AsDblDef(150.0),        // maxSat
                   args[10].AsDblDef(0.0),         // minSat
                   args[11].AsDblDef(16.0 / 1.19), // interp
                   args[12].AsBool(false),         // dither
                   args[13].AsBool(false),         // realcalc: force no-lookup (pure float calculation pixel)
                   args[14].AsDblDef(1.0),         // dither_strength 1.0 = +/-0.5 on the 0.255 range, scaled for others
                   env);
}
