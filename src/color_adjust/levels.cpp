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

#include "levels.h"
#include "kernel/common.h"
#include "kernel_adapter.h"
#include "kernel/levels.h"
Levels::Levels(PClip _child, float _in_min, double _gamma, float _in_max, float _out_min, float _out_max, bool _coring,
               bool _dither, IScriptEnvironment* env)
    : GenericVideoFilter(_child), cpu_mask_(color_cpu(env)), coring(_coring), dither(_dither), gamma(_gamma),
      in_min_f(_in_min), in_max_f(_in_max), out_min_f(_out_min), out_max_f(_out_max) {
  if (gamma <= 0.0)
    env->ThrowError("Levels: gamma must be positive");

  gamma = 1 / gamma;
  use_gamma = (gamma != 1.0);

  int in_min = (int)in_min_f;
  int in_max = (int)in_max_f;
  int out_min = (int)out_min_f;
  int out_max = (int)out_max_f;
  out_diff_f = out_max_f - out_min_f;

  int divisor;
  if (in_min == in_max)
    divisor = 1;
  else
    divisor = in_max - in_min;

  if (in_min_f == in_max_f)
    divisor_f = 1;
  else
    divisor_f = in_max_f - in_min_f;

  int scale = 1;
  //double bias = 0.0;

  dither_strength = 1.0f; // later: from parameter as Tweak

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent(); // 8,10..16

  // No lookup for float. Only slow pixel-by-pixel realtime calculation

  int lookup_size = pixelsize == 4 ? 0 : 1 << bits_per_pixel; // no LUT for float
  real_lookup_size =
      (pixelsize == 1) ? 256 : 65536; // avoids lut overflow in case of non-standard content of a 10 bit clip
  int max_pixel_value = pixelsize == 4 ? 1 : lookup_size - 1;

  use_lut = bits_per_pixel != 32; // for float: realtime only

  get_limits(limits, bits_per_pixel); // tv range limits

  if (pixelsize == 4)
    dither_strength /= 65536.0f; // same dither range as for a 16 bit clip

  if (dither) {
    // lut scale settings
    // same 256*dither for chroma and luma
    scale = 256; // lower 256 is dither value
    divisor *= 256;
    in_min *= 256;
    bias_dither = -(256.0f * dither_strength - 1) / 2; // -127.5 for 8 bit, scaling because of dithershift
  } else {
    scale = 1;
    bias_dither = 0.0f;
  }

  need_chroma = (vi.IsYUV() || vi.IsYUVA()) && !vi.IsY8();
  if (vi.IsRGB())
    coring = false; // no coring option for packed and planar RGBs

  // one buffer for map and mapchroma
  map = nullptr;
  if (use_lut) {
    int number_of_maps = need_chroma ? 2 : 1;
    int bufsize = pixelsize * real_lookup_size * scale * number_of_maps;
    map = static_cast<uint8_t*>(env->Allocate(bufsize, 16, AVS_NORMAL_ALLOC));
    if (!map)
      env->ThrowError("Levels: Could not reserve memory.");
    env->AtExit(free_buffer, map);
    if (bits_per_pixel > 8 && bits_per_pixel < 16) // make lut table safe for 10-14 bit garbage
      std::fill_n(map, bufsize, 0);                // 8 and 16 bit is safe

    if (need_chroma)
      mapchroma = map + pixelsize * real_lookup_size * scale; // pointer offset

    for (int i = 0; i < lookup_size * scale; ++i) {
      double p;

      int ii;
      if (dither)
        ii = (i & 0xFFFFFF00) + (int)((i & 0xFF) * dither_strength);
      else
        ii = i;

      if (coring)
        p = ((bias_dither + ii - limits.tv_range_low * scale) * ((double)max_pixel_value / limits.range_luma) -
             in_min) /
            divisor;
      else
        p = (bias_dither + ii - in_min) / divisor;

      p = pow(clamp(p, 0.0, 1.0), gamma);
      p = p * (out_max - out_min) + out_min;
      int luma;
      if (coring)
        luma = clamp(int(p * ((double)limits.range_luma / max_pixel_value) + limits.tv_range_low + 0.5),
                     limits.tv_range_low, limits.tv_range_hi_luma);
      else
        luma = clamp(int(p + 0.5), 0, max_pixel_value);

      if (pixelsize == 1)
        map[i] = (BYTE)luma;
      else // pixelsize==2
        reinterpret_cast<uint16_t*>(map)[i] = (uint16_t)luma;

      if (need_chroma) {
        int q = (int)(((bias_dither + ii - limits.middle_chroma * scale) * (out_max - out_min)) / divisor +
                      limits.middle_chroma + 0.5f);
        int chroma;
        if (coring)
          chroma = clamp(q, limits.tv_range_low, limits.tv_range_hi_chroma); // e.g. clamp(q, 16, 240)
        else
          chroma = clamp(q, 0, max_pixel_value); // e.g. clamp(q, 0, 255)
        if (pixelsize == 1)
          mapchroma[i] = (BYTE)chroma;
        else // pixelsize==2
          reinterpret_cast<uint16_t*>(mapchroma)[i] = (uint16_t)chroma;
      }
    }
  } else {
    // precalc float dither table from integer one
    if (dither) {
      for (int y = 0; y <= 15; y++)
        for (int x = 0; x <= 15; x++) {
          int index = (y << 4) | x; // 0..255
          ditherMap_f[index] = (ditherMap[index] / 255.0f - 0.5f) * dither_strength;
          // float dithering is 16 bit granularity, dither_strength is 1/65536.0 and not a parameter yet
        }
    }
  }
}

PVideoFrame __stdcall Levels::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame frame = child->GetFrame(n, env);
  env->MakeWritable(&frame);
  BYTE* p = frame->GetWritePtr();
  const int pitch = frame->GetPitch();

  if (use_lut && !dither) {
    if (vi.IsPlanar()) {
      const int rgb[3] = {PLANAR_G, PLANAR_B, PLANAR_R};
      const int yuv[3] = {PLANAR_Y, PLANAR_U, PLANAR_V};
      const bool isrgb = vi.IsRGB();
      const int* planes = isrgb ? rgb : yuv;
      for (int i = 0; i < (vi.IsY() ? 1 : 3); ++i) {
        int plane = planes[i];
        auto* data = frame->GetWritePtr(plane);
        int stride = frame->GetPitch(plane);
        map_channel(data, stride, data, stride, frame->GetRowSize(plane) / pixelsize, frame->GetHeight(plane),
                    (!isrgb && i) ? mapchroma : map, pixelsize == 1 ? 8 : 16, 1, env, cpu_mask_);
      }
    } else if (vi.IsYUY2()) {
      map_channel(p, pitch, p, pitch, vi.width, vi.height, map, 8, 2, env, cpu_mask_);
      map_channel(p + 1, pitch, p + 1, pitch, vi.width, vi.height, mapchroma, 8, 2, env, cpu_mask_);
    } else
      map_channel(p, pitch, p, pitch, frame->GetRowSize() / pixelsize, vi.height, map, pixelsize == 1 ? 8 : 16, 1, env,
                  cpu_mask_);
    return frame;
  }
  if (use_lut) {
    if (dither) {
      if (vi.IsYUY2()) {
        const int UVwidth = vi.width / 2;
        for (int y = 0; y < vi.height; ++y) {
          const int _y = (y << 4) & 0xf0;
          for (int x = 0; x < vi.width; ++x) {
            p[x * 2] = map[p[x * 2] << 8 | ditherMap[(x & 0x0f) | _y]];
          }
          for (int z = 0; z < UVwidth; ++z) {
            const int _dither = ditherMap[(z & 0x0f) | _y];
            p[z * 4 + 1] = mapchroma[p[z * 4 + 1] << 8 | _dither];
            p[z * 4 + 3] = mapchroma[p[z * 4 + 3] << 8 | _dither];
          }
          p += pitch;
        }
      } else if (vi.IsPlanar()) {
        if (vi.IsYUV() || vi.IsYUVA()) {
          // planar YUV
          if (pixelsize == 1) {
            for (int y = 0; y < vi.height; ++y) {
              const int _y = (y << 4) & 0xf0;
              for (int x = 0; x < vi.width; ++x) {
                p[x] = map[p[x] << 8 | ditherMap[(x & 0x0f) | _y]];
              }
              p += pitch;
            }
          } else { // pixelsize==2
            for (int y = 0; y < vi.height; ++y) {
              const int _y = (y << 4) & 0xf0;
              for (int x = 0; x < vi.width; ++x) {
                reinterpret_cast<uint16_t*>(p)[x] = reinterpret_cast<uint16_t*>(
                    map)[reinterpret_cast<uint16_t*>(p)[x] << 8 | ditherMap[(x & 0x0f) | _y]];
              }
              p += pitch;
            }
          }
          const int UVpitch = frame->GetPitch(PLANAR_U);
          const int w = frame->GetRowSize(PLANAR_U) / pixelsize;
          const int h = frame->GetHeight(PLANAR_U);
          p = frame->GetWritePtr(PLANAR_U);
          BYTE* q = frame->GetWritePtr(PLANAR_V);
          if (pixelsize == 1) {
            for (int y = 0; y < h; ++y) {
              const int _y = (y << 4) & 0xf0;
              for (int x = 0; x < w; ++x) {
                const int _dither = ditherMap[(x & 0x0f) | _y];
                p[x] = mapchroma[p[x] << 8 | _dither];
                q[x] = mapchroma[q[x] << 8 | _dither];
              }
              p += UVpitch;
              q += UVpitch;
            }
          } else { // pixelsize==2
            for (int y = 0; y < h; ++y) {
              const int _y = (y << 4) & 0xf0;
              for (int x = 0; x < w; ++x) {
                const int _dither = ditherMap[(x & 0x0f) | _y];
                reinterpret_cast<uint16_t*>(p)[x] =
                    reinterpret_cast<uint16_t*>(mapchroma)[reinterpret_cast<uint16_t*>(p)[x] << 8 | _dither];
                reinterpret_cast<uint16_t*>(q)[x] =
                    reinterpret_cast<uint16_t*>(mapchroma)[reinterpret_cast<uint16_t*>(q)[x] << 8 | _dither];
              }
              p += UVpitch;
              q += UVpitch;
            }
          }
        } else {
          // planar RGB
          BYTE* b = frame->GetWritePtr(PLANAR_B);
          BYTE* r = frame->GetWritePtr(PLANAR_R);
          const int pitch_b = frame->GetPitch(PLANAR_B);
          const int pitch_r = frame->GetPitch(PLANAR_R);
          if (pixelsize == 1) {
            for (int y = 0; y < vi.height; ++y) {
              const int _y = (y << 4) & 0xf0;
              for (int x = 0; x < vi.width; ++x) {
                p[x] = map[p[x] << 8 | ditherMap[(x & 0x0f) | _y]];
                b[x] = map[b[x] << 8 | ditherMap[(x & 0x0f) | _y]];
                r[x] = map[r[x] << 8 | ditherMap[(x & 0x0f) | _y]];
              }
              p += pitch;
              b += pitch_b;
              r += pitch_r;
            }
          } else { // pixelsize==2
            for (int y = 0; y < vi.height; ++y) {
              const int _y = (y << 4) & 0xf0;
              for (int x = 0; x < vi.width; ++x) {
                reinterpret_cast<uint16_t*>(p)[x] = reinterpret_cast<uint16_t*>(
                    map)[reinterpret_cast<uint16_t*>(p)[x] << 8 | ditherMap[(x & 0x0f) | _y]];
                reinterpret_cast<uint16_t*>(b)[x] = reinterpret_cast<uint16_t*>(
                    map)[reinterpret_cast<uint16_t*>(b)[x] << 8 | ditherMap[(x & 0x0f) | _y]];
                reinterpret_cast<uint16_t*>(r)[x] = reinterpret_cast<uint16_t*>(
                    map)[reinterpret_cast<uint16_t*>(r)[x] << 8 | ditherMap[(x & 0x0f) | _y]];
              }
              p += pitch;
              b += pitch_b;
              r += pitch_r;
            }
          }
        }
      } else if (vi.IsRGB32()) {
        for (int y = 0; y < vi.height; ++y) {
          const int _y = (y << 4) & 0xf0;
          for (int x = 0; x < vi.width; ++x) {
            const int _dither = ditherMap[(x & 0x0f) | _y];
            p[x * 4 + 0] = map[p[x * 4 + 0] << 8 | _dither];
            p[x * 4 + 1] = map[p[x * 4 + 1] << 8 | _dither];
            p[x * 4 + 2] = map[p[x * 4 + 2] << 8 | _dither];
            p[x * 4 + 3] = map[p[x * 4 + 3] << 8 | _dither];
          }
          p += pitch;
        }
      } else if (vi.IsRGB24()) {
        for (int y = 0; y < vi.height; ++y) {
          const int _y = (y << 4) & 0xf0;
          for (int x = 0; x < vi.width; ++x) {
            const int _dither = ditherMap[(x & 0x0f) | _y];
            p[x * 3 + 0] = map[p[x * 3 + 0] << 8 | _dither];
            p[x * 3 + 1] = map[p[x * 3 + 1] << 8 | _dither];
            p[x * 3 + 2] = map[p[x * 3 + 2] << 8 | _dither];
          }
          p += pitch;
        }
      } else if (vi.IsRGB64()) {
        for (int y = 0; y < vi.height; ++y) {
          const int _y = (y << 4) & 0xf0;
          for (int x = 0; x < vi.width; ++x) {
            const int _dither = ditherMap[(x & 0x0f) | _y];
            reinterpret_cast<uint16_t*>(p)[x * 4 + 0] =
                reinterpret_cast<uint16_t*>(map)[reinterpret_cast<uint16_t*>(p)[x * 4 + 0] << 8 | _dither];
            reinterpret_cast<uint16_t*>(p)[x * 4 + 1] =
                reinterpret_cast<uint16_t*>(map)[reinterpret_cast<uint16_t*>(p)[x * 4 + 1] << 8 | _dither];
            reinterpret_cast<uint16_t*>(p)[x * 4 + 2] =
                reinterpret_cast<uint16_t*>(map)[reinterpret_cast<uint16_t*>(p)[x * 4 + 2] << 8 | _dither];
            reinterpret_cast<uint16_t*>(p)[x * 4 + 3] =
                reinterpret_cast<uint16_t*>(map)[reinterpret_cast<uint16_t*>(p)[x * 4 + 3] << 8 | _dither];
          }
          p += pitch;
        }
      } else if (vi.IsRGB48()) {
        for (int y = 0; y < vi.height; ++y) {
          const int _y = (y << 4) & 0xf0;
          for (int x = 0; x < vi.width; ++x) {
            const int _dither = ditherMap[(x & 0x0f) | _y];
            reinterpret_cast<uint16_t*>(p)[x * 3 + 0] =
                reinterpret_cast<uint16_t*>(map)[reinterpret_cast<uint16_t*>(p)[x * 3 + 0] << 8 | _dither];
            reinterpret_cast<uint16_t*>(p)[x * 3 + 1] =
                reinterpret_cast<uint16_t*>(map)[reinterpret_cast<uint16_t*>(p)[x * 3 + 1] << 8 | _dither];
            reinterpret_cast<uint16_t*>(p)[x * 3 + 2] =
                reinterpret_cast<uint16_t*>(map)[reinterpret_cast<uint16_t*>(p)[x * 3 + 2] << 8 | _dither];
          }
          p += pitch;
        }
      }
    }
  } else {
    // float 32 bit. only planars here
    // no lut, realtime calculation only
    if (dither) {
      if (vi.IsYUV() || vi.IsYUVA()) { // planar YUV (incl Y only)
        // luma. with or w/o gamma
        if (use_gamma) {
          for (int y = 0; y < vi.height; ++y) {
            const int _y = (y << 4) & 0xf0;
            for (int x = 0; x < vi.width; ++x) {
              float _dither = ditherMap_f[(x & 0x0f) | _y];
              const float pixel = reinterpret_cast<float*>(p)[x] + _dither;
              reinterpret_cast<float*>(p)[x] = calcPixel<false, true>(pixel);
            }
            p += pitch;
          }
        } else {
          // don't use gamma (faster)
          for (int y = 0; y < vi.height; ++y) {
            const int _y = (y << 4) & 0xf0;
            for (int x = 0; x < vi.width; ++x) {
              float _dither = ditherMap_f[(x & 0x0f) | _y];
              const float pixel = reinterpret_cast<float*>(p)[x] + _dither;
              reinterpret_cast<float*>(p)[x] = calcPixel<false, false>(pixel); // w/o gamma
            }
            p += pitch;
          }
        }
        // chroma
        if (need_chroma) {
          const int UVpitch = frame->GetPitch(PLANAR_U);
          const int w = frame->GetRowSize(PLANAR_U) / pixelsize;
          const int h = frame->GetHeight(PLANAR_U);
          p = frame->GetWritePtr(PLANAR_U);
          BYTE* q = frame->GetWritePtr(PLANAR_V);
          for (int y = 0; y < h; ++y) {
            const int _y = (y << 4) & 0xf0;
            for (int x = 0; x < w; ++x) {
              float _dither = ditherMap_f[(x & 0x0f) | _y];
              const float pixel_u = reinterpret_cast<float*>(p)[x] + _dither;
              reinterpret_cast<float*>(p)[x] = calcPixel<true, false>(pixel_u);
              const float pixel_v = reinterpret_cast<float*>(q)[x] + _dither;
              reinterpret_cast<float*>(q)[x] = calcPixel<true, false>(pixel_v);
            }
            p += UVpitch;
            q += UVpitch;
          }
        }
      } else if (vi.IsPlanarRGB() || vi.IsPlanarRGBA()) {
        // planar RGB
        BYTE* b = frame->GetWritePtr(PLANAR_B);
        BYTE* r = frame->GetWritePtr(PLANAR_R);
        const int pitch_b = frame->GetPitch(PLANAR_B);
        const int pitch_r = frame->GetPitch(PLANAR_R);
        if (use_gamma) {
          for (int y = 0; y < vi.height; ++y) {
            const int _y = (y << 4) & 0xf0;
            for (int x = 0; x < vi.width; ++x) {
              float _dither = ditherMap_f[(x & 0x0f) | _y];
              const float pixel_p = reinterpret_cast<float*>(p)[x] + _dither; // g channel
              reinterpret_cast<float*>(p)[x] = calcPixel<false, true>(pixel_p);
              const float pixel_b = reinterpret_cast<float*>(b)[x] + _dither;
              reinterpret_cast<float*>(b)[x] = calcPixel<false, true>(pixel_b);
              const float pixel_r = reinterpret_cast<float*>(r)[x] + _dither;
              reinterpret_cast<float*>(r)[x] = calcPixel<false, true>(pixel_r);
            }
            p += pitch; // g
            b += pitch_b;
            r += pitch_r;
          }
        } else {
          for (int y = 0; y < vi.height; ++y) {
            const int _y = (y << 4) & 0xf0;
            for (int x = 0; x < vi.width; ++x) {
              float _dither = ditherMap_f[(x & 0x0f) | _y];
              const float pixel_p = reinterpret_cast<float*>(p)[x] + _dither; // g channel
              reinterpret_cast<float*>(p)[x] = calcPixel<false, false>(pixel_p);
              const float pixel_b = reinterpret_cast<float*>(b)[x] + _dither;
              reinterpret_cast<float*>(b)[x] = calcPixel<false, false>(pixel_b);
              const float pixel_r = reinterpret_cast<float*>(r)[x] + _dither;
              reinterpret_cast<float*>(r)[x] = calcPixel<false, false>(pixel_r);
            }
            p += pitch; // g
            b += pitch_b;
            r += pitch_r;
          }
        }
      } else {
        // 32 bit, neither YUV(A), nor RGB(A)
      }
    } else {
      // no dither
      if (vi.IsYUV() || vi.IsYUVA()) { // planar YUV (incl Y only)
                                       // luma. with or w/o gamma
        if (use_gamma) {
          for (int y = 0; y < vi.height; ++y) {
            for (int x = 0; x < vi.width; ++x) {
              const float pixel = reinterpret_cast<float*>(p)[x];
              reinterpret_cast<float*>(p)[x] = calcPixel<false, true>(pixel);
            }
            p += pitch;
          }
        } else {
          // don't use gamma (faster)
          for (int y = 0; y < vi.height; ++y) {
            for (int x = 0; x < vi.width; ++x) {
              const float pixel = reinterpret_cast<float*>(p)[x];
              reinterpret_cast<float*>(p)[x] = calcPixel<false, false>(pixel); // w/o gamma
            }
            p += pitch;
          }
        }
        // chroma
        if (need_chroma) {
          const int UVpitch = frame->GetPitch(PLANAR_U);
          const int w = frame->GetRowSize(PLANAR_U) / pixelsize;
          const int h = frame->GetHeight(PLANAR_U);
          p = frame->GetWritePtr(PLANAR_U);
          BYTE* q = frame->GetWritePtr(PLANAR_V);
          for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
              const float pixel_u = reinterpret_cast<float*>(p)[x];
              reinterpret_cast<float*>(p)[x] = calcPixel<true, false>(pixel_u);
              const float pixel_v = reinterpret_cast<float*>(q)[x];
              reinterpret_cast<float*>(q)[x] = calcPixel<true, false>(pixel_v);
            }
            p += UVpitch;
            q += UVpitch;
          }
        }
      } else if (vi.IsPlanarRGB() || vi.IsPlanarRGBA()) {
        // planar RGB
        BYTE* b = frame->GetWritePtr(PLANAR_B);
        BYTE* r = frame->GetWritePtr(PLANAR_R);
        const int pitch_b = frame->GetPitch(PLANAR_B);
        const int pitch_r = frame->GetPitch(PLANAR_R);
        if (use_gamma) {
          for (int y = 0; y < vi.height; ++y) {
            for (int x = 0; x < vi.width; ++x) {
              const float pixel_p = reinterpret_cast<float*>(p)[x]; // g channel
              reinterpret_cast<float*>(p)[x] = calcPixel<false, true>(pixel_p);
              const float pixel_b = reinterpret_cast<float*>(b)[x];
              reinterpret_cast<float*>(b)[x] = calcPixel<false, true>(pixel_b);
              const float pixel_r = reinterpret_cast<float*>(r)[x];
              reinterpret_cast<float*>(r)[x] = calcPixel<false, true>(pixel_r);
            }
            p += pitch; // g
            b += pitch_b;
            r += pitch_r;
          }
        } else {
          for (int y = 0; y < vi.height; ++y) {
            for (int x = 0; x < vi.width; ++x) {
              const float pixel_p = reinterpret_cast<float*>(p)[x]; // g channel
              reinterpret_cast<float*>(p)[x] = calcPixel<false, false>(pixel_p);
              const float pixel_b = reinterpret_cast<float*>(b)[x];
              reinterpret_cast<float*>(b)[x] = calcPixel<false, false>(pixel_b);
              const float pixel_r = reinterpret_cast<float*>(r)[x];
              reinterpret_cast<float*>(r)[x] = calcPixel<false, false>(pixel_r);
            }
            p += pitch; // g
            b += pitch_b;
            r += pitch_r;
          }
        }
      } else {
        // 32 bit, neither YUV(A), nor RGB(A)
      }
    }
  }
  return frame;
}

AVSValue __cdecl Levels::Create(AVSValue args, void*, IScriptEnvironment* env) {
  enum { CHILD, IN_MIN, GAMMA, IN_MAX, OUT_MIN, OUT_MAX, CORING, DITHER };
  return new Levels(args[CHILD].AsClip(), (float)args[IN_MIN].AsFloat(), (float)args[GAMMA].AsFloat(),
                    (float)args[IN_MAX].AsFloat(), (float)args[OUT_MIN].AsFloat(), (float)args[OUT_MAX].AsFloat(),
                    args[CORING].AsBool(true), args[DITHER].AsBool(false), env);
}
