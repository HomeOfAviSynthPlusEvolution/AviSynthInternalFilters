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

#include "rgb_adjust.h"
#include "kernel/common.h"
#include "kernel_adapter.h"
#define READ_CONDITIONAL(plane_num, var_name, internal_name, condVarSuffix)                                            \
  {                                                                                                                    \
    std::string s = "rgbadjust_" #var_name;                                                                            \
    s = s + condVarSuffix;                                                                                             \
    const double t = env->GetVarDouble(s.c_str(), DBL_MIN);                                                            \
    if (t != DBL_MIN) {                                                                                                \
      config->rgba[plane_num].internal_name = t;                                                                       \
      config->rgba[plane_num].changed = true;                                                                          \
    }                                                                                                                  \
  }

static void rgbadjust_read_conditional(IScriptEnvironment* env, RGBAdjustConfig* config, const char* condVarSuffix) {
  READ_CONDITIONAL(0, r, scale, condVarSuffix);
  READ_CONDITIONAL(1, g, scale, condVarSuffix);
  READ_CONDITIONAL(2, b, scale, condVarSuffix);
  READ_CONDITIONAL(3, a, scale, condVarSuffix);

  READ_CONDITIONAL(0, rb, bias, condVarSuffix);
  READ_CONDITIONAL(1, gb, bias, condVarSuffix);
  READ_CONDITIONAL(2, bb, bias, condVarSuffix);
  READ_CONDITIONAL(3, ab, bias, condVarSuffix);

  READ_CONDITIONAL(0, rg, gamma, condVarSuffix);
  READ_CONDITIONAL(1, gg, gamma, condVarSuffix);
  READ_CONDITIONAL(2, bg, gamma, condVarSuffix);
  READ_CONDITIONAL(3, ag, gamma, condVarSuffix);
}

#undef READ_CONDITIONAL

RGBAdjust::~RGBAdjust() {
  if (map_holder)
    delete[] map_holder;
}

void RGBAdjust::CheckAndConvertParams(RGBAdjustConfig& config, IScriptEnvironment* env) {
  if ((config.rgba[0].gamma <= 0.0) || (config.rgba[1].gamma <= 0.0) || (config.rgba[2].gamma <= 0.0) ||
      (config.rgba[3].gamma <= 0.0))
    env->ThrowError("RGBAdjust: gammas must be positive");
}

static __inline float RGBAdjust_processPixel(const float val, const double c0, const double c1, const double c2) {
  const double pixel_max = 1.0;
  return (float)(pow(clamp((c0 + val * c1) / pixel_max, 0.0, 1.0), c2));
}

void RGBAdjust::rgbadjust_create_lut(BYTE* lut_buf, const int plane, RGBAdjustConfig& cfg) {
  if (!use_lut)
    return;

  const int lookup_size = 1 << bits_per_pixel; // 256, 1024, 4096, 16384, 65536

  void (*set_map)(BYTE*, int, int, float, const double, const double, const double);
  if (dither) {
    set_map = [](BYTE* map, int lookup_size, int bits_per_pixel, float dither_strength, const double c0,
                 const double c1, const double c2) {
      double bias_dither = -(256.0f * dither_strength - 1) / 2; // -127.5 for 8 bit, scaling because of dithershift
      double pixel_max = (1 << bits_per_pixel) - 1;
      if (bits_per_pixel == 8) {
        for (int i = 0; i < lookup_size * 256; ++i) {
          int ii = (i & 0xFFFFFF00) + (int)((i & 0xFF) * dither_strength);
          map[i] = BYTE(pow(clamp((c0 * 256 + ii * c1 - bias_dither) / (double(pixel_max) * 256), 0.0, 1.0), c2) *
                            (double)pixel_max +
                        0.5);
        }
      } else {
        for (int i = 0; i < lookup_size * 256; ++i) {
          int ii = (i & 0xFFFFFF00) + (int)((i & 0xFF) * dither_strength);
          reinterpret_cast<uint16_t*>(map)[i] =
              uint16_t(pow(clamp((c0 * 256 + ii * c1 - bias_dither) / (double(pixel_max) * 256), 0.0, 1.0), c2) *
                           (double)pixel_max +
                       0.5);
        }
      }
    };
  } else {
    set_map = [](BYTE* map, int lookup_size, int bits_per_pixel, float dither_strength, const double c0,
                 const double c1, const double c2) {
      double pixel_max = (1 << bits_per_pixel) - 1;
      if (bits_per_pixel == 8) {
        for (int i = 0; i < lookup_size; ++i) { // fix of bug introduced in an earlier refactor was: i < 256 * 256
          map[i] = BYTE(pow(clamp((c0 + i * c1) / (double)pixel_max, 0.0, 1.0), c2) * double(pixel_max) + 0.5);
        }
      } else {
        for (int i = 0; i < lookup_size; ++i) { // fix of bug introduced in an earlier refactor was: i < 256 * 256
          reinterpret_cast<uint16_t*>(map)[i] =
              uint16_t(pow(clamp((c0 + i * c1) / (double)pixel_max, 0.0, 1.0), c2) * double(pixel_max) + 0.5);
        }
      }
    };
  }

  set_map(lut_buf, lookup_size, bits_per_pixel, dither_strength, cfg.rgba[plane].bias, cfg.rgba[plane].scale,
          1 / cfg.rgba[plane].gamma);
}

RGBAdjust::RGBAdjust(PClip _child, double r, double g, double b, double a, double rb, double gb, double bb, double ab,
                     double rg, double gg, double bg, double ag, bool _analyze, bool _dither, bool _conditional,
                     const char* _condVarSuffix, IScriptEnvironment* env)
    : GenericVideoFilter(_child), analyze(_analyze), dither(_dither), conditional(_conditional),
      condVarSuffix(_condVarSuffix) {
  // one buffer for all maps
  map_holder = nullptr;

  if (!vi.IsRGB())
    env->ThrowError("RGBAdjust requires RGB input");

  config.rgba[0].scale = r;
  config.rgba[1].scale = g;
  config.rgba[2].scale = b;
  config.rgba[3].scale = a;
  // bias
  config.rgba[0].bias = rb;
  config.rgba[1].bias = gb;
  config.rgba[2].bias = bb;
  config.rgba[3].bias = ab;
  // gammas
  config.rgba[0].gamma = rg;
  config.rgba[1].gamma = gg;
  config.rgba[2].gamma = bg;
  config.rgba[3].gamma = ag;

  config.rgba[0].changed = false;
  config.rgba[1].changed = false;
  config.rgba[2].changed = false;
  config.rgba[3].changed = false;

  CheckAndConvertParams(config, env);

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent(); // 8,10..16

  if (pixelsize == 4) {
    // dither parameter is silently ignored
    // if (dither) env->ThrowError("RGBAdjust: cannot 'dither' a 32bit float video");
  }
  // No lookup for float. todo: slow on-the-fly realtime calculation

  real_lookup_size =
      (pixelsize == 1) ? 256 : 65536; // avoids lut overflow in case of non-standard content of a 10 bit clip
  max_pixel_value = (pixelsize == 4) ? 255 : (1 << bits_per_pixel) - 1; // n/a for float formats
  dither_strength = 1.0f;                                               // fixed, not used

  use_lut = bits_per_pixel != 32; // for float: realtime (todo)

  if (!use_lut)
    dither = false;

  if (use_lut) {
    number_of_maps = (vi.IsRGB24() || vi.IsRGB48() || vi.IsPlanarRGB()) ? 3 : 4;
    int one_bufsize = pixelsize * real_lookup_size;
    if (dither)
      one_bufsize *= 256;

    map_holder = new uint8_t[one_bufsize * number_of_maps];
    /*
      // left here intentionally:
      // for some reason, AtExit does not get called from within ScriptClip, causing no free thus memory leak
      // We are using new here and delete in destructor
      static_cast<uint8_t*>(env->Allocate(one_bufsize * number_of_maps, 16, AVS_NORMAL_ALLOC));
      if (!mapR)
          env->ThrowError("RGBAdjust: Could not reserve memory.");
      env->AtExit(free_buffer, mapR);
      */
    if (bits_per_pixel > 8 && bits_per_pixel < 16)              // make lut table safe for 10-14 bit garbage
      std::fill_n(map_holder, one_bufsize * number_of_maps, 0); // 8 and 16 bit fully overwrites
    maps[0] = map_holder;
    maps[1] = maps[0] + one_bufsize;
    maps[2] = maps[1] + one_bufsize;
    maps[3] = number_of_maps == 4 ? maps[2] + one_bufsize : nullptr;

    for (int plane = 0; plane < number_of_maps; plane++) {
      rgbadjust_create_lut(maps[plane], plane, config);
    }
  }
}

#include "kernel/rgb_adjust.h"
PVideoFrame __stdcall RGBAdjust::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame frame = child->GetFrame(n, env);
  env->MakeWritable(&frame);
  BYTE* p = frame->GetWritePtr();
  const int pitch = frame->GetPitch();

  int w = vi.width;
  int h = vi.height;

  RGBAdjustConfig local_config = config;

  // Read conditional variables
  local_config.rgba[0].changed = false;
  local_config.rgba[1].changed = false;
  local_config.rgba[2].changed = false;
  local_config.rgba[3].changed = false;
  if (conditional)
    rgbadjust_read_conditional(env, &local_config, condVarSuffix);

  BYTE* maps_live[4] = {nullptr};
  BYTE* maps_local[4] = {nullptr}; // for local lut table allocation, don't overwrite common buffer
  for (int i = 0; i < 4; i++)
    maps_live[i] = maps[i];

  if (local_config.rgba[0].changed || local_config.rgba[1].changed || local_config.rgba[2].changed ||
      local_config.rgba[3].changed) {
    CheckAndConvertParams(local_config, env);
    if (use_lut) {
      for (int plane = 0; plane < (int)number_of_maps; plane++) {
        // recalculate plane LUT only if changed
        if (local_config.rgba[plane].changed) {
          size_t local_lut_size = static_cast<size_t>(pixelsize) * real_lookup_size;
          if (dither)
            local_lut_size *= 256;
          maps_local[plane] = new BYTE[local_lut_size];
          maps_live[plane] = maps_local[plane]; // use our new local lut
          rgbadjust_create_lut(maps_live[plane], plane, local_config);
        }
      }
    }
  }

  if (dither) {
    if (vi.IsRGB32())
      apply_map_rgb_packed_c<uint8_t, 4, true>(p, pitch, maps_live[0], maps_live[1], maps_live[2], maps_live[3], w, h,
                                               bits_per_pixel, env);
    else if (vi.IsRGB24())
      apply_map_rgb_packed_c<uint8_t, 3, true>(p, pitch, maps_live[0], maps_live[1], maps_live[2], maps_live[3], w, h,
                                               bits_per_pixel, env);
    else if (vi.IsRGB64())
      apply_map_rgb_packed_c<uint16_t, 4, true>(p, pitch, maps_live[0], maps_live[1], maps_live[2], maps_live[3], w, h,
                                                bits_per_pixel, env);
    else if (vi.IsRGB48())
      apply_map_rgb_packed_c<uint16_t, 3, true>(p, pitch, maps_live[0], maps_live[1], maps_live[2], maps_live[3], w, h,
                                                bits_per_pixel, env);
    else {
      // Planar RGB
      bool hasAlpha = vi.IsPlanarRGBA();
      BYTE* p_g = p;
      BYTE* p_b = frame->GetWritePtr(PLANAR_B);
      BYTE* p_r = frame->GetWritePtr(PLANAR_R);
      BYTE* p_a = frame->GetWritePtr(PLANAR_A);
      // no float support
      if (pixelsize == 1) {
        if (hasAlpha)
          apply_map_rgb_planar_c<uint8_t, true, true>(p_r, p_g, p_b, p_a, pitch, maps_live[0], maps_live[1],
                                                      maps_live[2], maps_live[3], w, h, bits_per_pixel, env);
        else
          apply_map_rgb_planar_c<uint8_t, false, true>(p_r, p_g, p_b, p_a, pitch, maps_live[0], maps_live[1],
                                                       maps_live[2], maps_live[3], w, h, bits_per_pixel, env);
      } else {
        if (hasAlpha)
          apply_map_rgb_planar_c<uint16_t, true, true>(p_r, p_g, p_b, p_a, pitch, maps_live[0], maps_live[1],
                                                       maps_live[2], maps_live[3], w, h, bits_per_pixel, env);
        else
          apply_map_rgb_planar_c<uint16_t, false, true>(p_r, p_g, p_b, p_a, pitch, maps_live[0], maps_live[1],
                                                        maps_live[2], maps_live[3], w, h, bits_per_pixel, env);
      }
    }
  } else {
    // no dither
    if (vi.IsRGB32())
      apply_map_rgb_packed_c<uint8_t, 4, false>(p, pitch, maps_live[0], maps_live[1], maps_live[2], maps_live[3], w, h,
                                                bits_per_pixel, env);
    else if (vi.IsRGB24())
      apply_map_rgb_packed_c<uint8_t, 3, false>(p, pitch, maps_live[0], maps_live[1], maps_live[2], maps_live[3], w, h,
                                                bits_per_pixel, env);
    else if (vi.IsRGB64())
      apply_map_rgb_packed_c<uint16_t, 4, false>(p, pitch, maps_live[0], maps_live[1], maps_live[2], maps_live[3], w, h,
                                                 bits_per_pixel, env);
    else if (vi.IsRGB48())
      apply_map_rgb_packed_c<uint16_t, 3, false>(p, pitch, maps_live[0], maps_live[1], maps_live[2], maps_live[3], w, h,
                                                 bits_per_pixel, env);
    else {
      // Planar RGB
      bool hasAlpha = vi.IsPlanarRGBA();
      BYTE* p_g = p;
      BYTE* p_b = frame->GetWritePtr(PLANAR_B);
      BYTE* p_r = frame->GetWritePtr(PLANAR_R);
      BYTE* p_a = frame->GetWritePtr(PLANAR_A);
      // no float support
      if (pixelsize == 1) {
        if (hasAlpha)
          apply_map_rgb_planar_c<uint8_t, true, false>(p_r, p_g, p_b, p_a, pitch, maps_live[0], maps_live[1],
                                                       maps_live[2], maps_live[3], w, h, bits_per_pixel, env);
        else
          apply_map_rgb_planar_c<uint8_t, false, false>(p_r, p_g, p_b, p_a, pitch, maps_live[0], maps_live[1],
                                                        maps_live[2], maps_live[3], w, h, bits_per_pixel, env);
      } else if (pixelsize == 2) {
        if (hasAlpha)
          apply_map_rgb_planar_c<uint16_t, true, false>(p_r, p_g, p_b, p_a, pitch, maps_live[0], maps_live[1],
                                                        maps_live[2], maps_live[3], w, h, bits_per_pixel, env);
        else
          apply_map_rgb_planar_c<uint16_t, false, false>(p_r, p_g, p_b, p_a, pitch, maps_live[0], maps_live[1],
                                                         maps_live[2], maps_live[3], w, h, bits_per_pixel, env);
      } else {
        // 32 bit float, no dither
        const int planesRGB_RgbaOrder[4] = {PLANAR_R, PLANAR_G, PLANAR_B, PLANAR_A};
        const int* planes = planesRGB_RgbaOrder;

        for (int cplane = 0; cplane < (hasAlpha ? 4 : 3); cplane++) {
          RGBAdjustPlaneConfig x = local_config.rgba[cplane];
          const double scale = x.scale;
          const double bias = x.bias;
          const double gamma = 1 / x.gamma;
          int plane = planes[cplane];
          float* dstp = reinterpret_cast<float*>(frame->GetWritePtr(plane));
          int pitch = frame->GetPitch(plane) / sizeof(float);
          for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
              dstp[x] = RGBAdjust_processPixel(dstp[x], bias, scale, gamma);
            }
            dstp += pitch;
          }
        }
      }
    }
  }

  if (use_lut) {
    for (int i = 0; i < 4; i++)
      if (maps_local[i])
        delete[] maps_local[i];
  }

  if (analyze) {
    const int w = frame->GetRowSize() / pixelsize;
    const int h = frame->GetHeight();

    const int analyze_lookup_size =
        pixelsize == 4 ? 1 << 16 : 1 << bits_per_pixel; // 32 bit float is quantized to 16 bits
    const int max_pixel_value_analyze = analyze_lookup_size - 1;

    // allocate 3x bufsize for R. G and B will share it
    auto accum_r =
        static_cast<uint32_t*>(env->Allocate(analyze_lookup_size * sizeof(uint32_t) * 3, 16, AVS_NORMAL_ALLOC));
    auto accum_g = accum_r + analyze_lookup_size;
    auto accum_b = accum_g + analyze_lookup_size;
    if (!accum_r)
      env->ThrowError("RGBAdjust: Could not reserve memory.");

    for (int i = 0; i < analyze_lookup_size; i++) {
      accum_r[i] = 0;
      accum_g[i] = 0;
      accum_b[i] = 0;
    }

    const int pixels = vi.width * vi.height;

    if (bits_per_pixel == 32) {
      RGBStats rgbplanedata;
      const BYTE* p_g = frame->GetReadPtr(PLANAR_G);
      ;
      const BYTE* p_b = frame->GetReadPtr(PLANAR_B);
      const BYTE* p_r = frame->GetReadPtr(PLANAR_R);
      fill_accum_rgb_planar_float_c(p_r, p_g, p_b, pitch, accum_r, accum_g, accum_b, w, h, rgbplanedata);

      double avg_r = rgbplanedata.data[0].sum / pixels;
      double avg_g = rgbplanedata.data[1].sum / pixels;
      double avg_b = rgbplanedata.data[2].sum / pixels;

      int Amin_r = 0, Amin_g = 0, Amin_b = 0;
      int Amax_r = 0, Amax_g = 0, Amax_b = 0;
      bool Ahit_minr = false, Ahit_ming = false, Ahit_minb = false;
      bool Ahit_maxr = false, Ahit_maxg = false, Ahit_maxb = false;
      int At_256 = (pixels + 128) / 256; // When 1/256th of all pixels have been reached, trigger "Loose min/max"

      double st_r = 0, st_g = 0, st_b = 0;

      for (int i = 0; i < analyze_lookup_size; i++) {
        double i_float = i / 65535.0;
        st_r += accum_r[i] * (i_float - avg_r) * (i_float - avg_r);
        st_g += accum_g[i] * (i_float - avg_g) * (i_float - avg_g);
        st_b += accum_b[i] * (i_float - avg_b) * (i_float - avg_b);

        // loose min
        if (!Ahit_minr) {
          Amin_r += accum_r[i];
          if (Amin_r > At_256) {
            Ahit_minr = true;
            Amin_r = i;
          }
        }
        if (!Ahit_ming) {
          Amin_g += accum_g[i];
          if (Amin_g > At_256) {
            Ahit_ming = true;
            Amin_g = i;
          }
        }
        if (!Ahit_minb) {
          Amin_b += accum_b[i];
          if (Amin_b > At_256) {
            Ahit_minb = true;
            Amin_b = i;
          }
        }
        // loose max
        if (!Ahit_maxr) {
          Amax_r += accum_r[max_pixel_value_analyze - i];
          if (Amax_r > At_256) {
            Ahit_maxr = true;
            Amax_r = max_pixel_value_analyze - i;
          }
        }
        if (!Ahit_maxg) {
          Amax_g += accum_g[max_pixel_value_analyze - i];
          if (Amax_g > At_256) {
            Ahit_maxg = true;
            Amax_g = max_pixel_value_analyze - i;
          }
        }
        if (!Ahit_maxb) {
          Amax_b += accum_b[max_pixel_value_analyze - i];
          if (Amax_b > At_256) {
            Ahit_maxb = true;
            Amax_b = max_pixel_value_analyze - i;
          }
        }
      }

      auto Fst_r = sqrt(st_r / pixels);
      auto Fst_g = sqrt(st_g / pixels);
      auto Fst_b = sqrt(st_b / pixels);

      char text[512];
      const bool StatsAsInteger16 = false;
      if (StatsAsInteger16)
        sprintf(text,
                "At 16 bits.  Frame: %-8u (   Red   /  Green  /  Blue   )\n"
                "           Average:      ( %7.2f / %7.2f / %7.2f )\n"
                "Standard Deviation:      ( %7.2f / %7.2f / %7.2f )\n"
                "           Minimum:      ( %7.2f / %7.2f / %7.2f )\n"
                "           Maximum:      ( %7.2f / %7.2f / %7.2f )\n"
                "     Loose Minimum:      ( %5d    / %5d    / %5d    )\n"
                "     Loose Maximum:      ( %5d    / %5d    / %5d    )\n",
                (unsigned int)n, avg_r * 65535, avg_g * 65535, avg_b * 65535, Fst_r * 65535, Fst_g * 65535,
                Fst_b * 65535, rgbplanedata.data[0].real_min * 65535, rgbplanedata.data[1].real_min * 65535,
                rgbplanedata.data[2].real_min * 65535, rgbplanedata.data[0].real_max * 65535,
                rgbplanedata.data[1].real_max * 65535, rgbplanedata.data[2].real_max * 65535, Amin_r, Amin_g, Amin_b,
                Amax_r, Amax_g, Amax_b);
      else // stats as Float
        sprintf(text,
                "             Frame: %-8u (   Red   /  Green  /  Blue   )\n"
                "           Average:      ( %7.5f / %7.5f / %7.5f )\n"
                "Standard Deviation:      ( %7.5f / %7.5f / %7.5f )\n"
                "           Minimum:      ( %7.5f / %7.5f / %7.5f )\n"
                "           Maximum:      ( %7.5f / %7.5f / %7.5f )\n"
                "     Loose Minimum:      ( %7.5f / %7.5f / %7.5f )\n"
                "     Loose Maximum:      ( %7.5f / %7.5f / %7.5f )\n",
                (unsigned int)n, avg_r, avg_g, avg_b, Fst_r, Fst_g, Fst_b, rgbplanedata.data[0].real_min,
                rgbplanedata.data[1].real_min, rgbplanedata.data[2].real_min, rgbplanedata.data[0].real_max,
                rgbplanedata.data[1].real_max, rgbplanedata.data[2].real_max, Amin_r / 65535.0, Amin_g / 65535.0,
                Amin_b / 65535.0, Amax_r / 65535.0, Amax_g / 65535.0, Amax_b / 65535.0);
      env->ApplyMessage(&frame, vi, text, vi.width / 4, 0xa0a0a0, 0, 0);
    } else {

      if (vi.IsPlanarRGB() || vi.IsPlanarRGBA()) {
        const BYTE* p_g = frame->GetReadPtr(PLANAR_G);
        ;
        const BYTE* p_b = frame->GetReadPtr(PLANAR_B);
        const BYTE* p_r = frame->GetReadPtr(PLANAR_R);
        if (bits_per_pixel == 8)
          fill_accum_rgb_planar_c<uint8_t>(p_r, p_g, p_b, pitch, accum_r, accum_g, accum_b, w, h,
                                           max_pixel_value_analyze);
        else if (bits_per_pixel <= 16)
          fill_accum_rgb_planar_c<uint16_t>(p_r, p_g, p_b, pitch, accum_r, accum_g, accum_b, w, h,
                                            max_pixel_value_analyze);
        else // 32 bit float
          ;  // handled in other branch;
      } else {
        // packed RGB
        const BYTE* srcp = frame->GetReadPtr();
        const int pixel_step = vi.IsRGB24() || vi.IsRGB48() ? 3 : 4;

        if (pixelsize == 1)
          fill_accum_rgb_packed_c<uint8_t>(srcp, pitch, accum_r, accum_g, accum_b, w, h, pixel_step);
        else
          fill_accum_rgb_packed_c<uint16_t>(srcp, pitch, accum_r, accum_g, accum_b, w, h, pixel_step);
      }

      double avg_r = 0, avg_g = 0, avg_b = 0;
      double st_r = 0, st_g = 0, st_b = 0;
      int min_r = 0, min_g = 0, min_b = 0;
      int max_r = 0, max_g = 0, max_b = 0;
      bool hit_r = false, hit_g = false, hit_b = false;
      int Amin_r = 0, Amin_g = 0, Amin_b = 0;
      int Amax_r = 0, Amax_g = 0, Amax_b = 0;
      bool Ahit_minr = false, Ahit_ming = false, Ahit_minb = false;
      bool Ahit_maxr = false, Ahit_maxg = false, Ahit_maxb = false;
      int At_256 = (pixels + 128) / 256; // When 1/256th of all pixels have been reached, trigger "Loose min/max"

      for (int i = 0; i < analyze_lookup_size; i++) {
        avg_r += (double)accum_r[i] * i;
        avg_g += (double)accum_g[i] * i;
        avg_b += (double)accum_b[i] * i;

        if (accum_r[i] != 0) {
          max_r = i;
          hit_r = true;
        } else {
          if (!hit_r)
            min_r = i + 1;
        }
        if (accum_g[i] != 0) {
          max_g = i;
          hit_g = true;
        } else {
          if (!hit_g)
            min_g = i + 1;
        }
        if (accum_b[i] != 0) {
          max_b = i;
          hit_b = true;
        } else {
          if (!hit_b)
            min_b = i + 1;
        }

        if (!Ahit_minr) {
          Amin_r += accum_r[i];
          if (Amin_r > At_256) {
            Ahit_minr = true;
            Amin_r = i;
          }
        }
        if (!Ahit_ming) {
          Amin_g += accum_g[i];
          if (Amin_g > At_256) {
            Ahit_ming = true;
            Amin_g = i;
          }
        }
        if (!Ahit_minb) {
          Amin_b += accum_b[i];
          if (Amin_b > At_256) {
            Ahit_minb = true;
            Amin_b = i;
          }
        }

        if (!Ahit_maxr) {
          Amax_r += accum_r[max_pixel_value_analyze - i];
          if (Amax_r > At_256) {
            Ahit_maxr = true;
            Amax_r = max_pixel_value_analyze - i;
          }
        }
        if (!Ahit_maxg) {
          Amax_g += accum_g[max_pixel_value_analyze - i];
          if (Amax_g > At_256) {
            Ahit_maxg = true;
            Amax_g = max_pixel_value_analyze - i;
          }
        }
        if (!Ahit_maxb) {
          Amax_b += accum_b[max_pixel_value_analyze - i];
          if (Amax_b > At_256) {
            Ahit_maxb = true;
            Amax_b = max_pixel_value_analyze - i;
          }
        }
      }

      float Favg_r = (float)(avg_r / pixels);
      float Favg_g = (float)(avg_g / pixels);
      float Favg_b = (float)(avg_b / pixels);

      for (int i = 0; i < analyze_lookup_size; i++) {
        st_r += (float)accum_r[i] * (float(i - Favg_r) * (i - Favg_r));
        st_g += (float)accum_g[i] * (float(i - Favg_g) * (i - Favg_g));
        st_b += (float)accum_b[i] * (float(i - Favg_b) * (i - Favg_b));
      }

      float Fst_r = (float)sqrt(st_r / pixels);
      float Fst_g = (float)sqrt(st_g / pixels);
      float Fst_b = (float)sqrt(st_b / pixels);

      char text[512];
      if (bits_per_pixel == 8)
        sprintf(text,
                "             Frame: %-8u (  Red  / Green / Blue  )\n"
                "           Average:      ( %5.2f / %5.2f / %5.2f )\n"
                "Standard Deviation:      ( %5.2f / %5.2f / %5.2f )\n"
                "           Minimum:      ( %3d    / %3d    / %3d    )\n"
                "           Maximum:      ( %3d    / %3d    / %3d    )\n"
                "     Loose Minimum:      ( %3d    / %3d    / %3d    )\n"
                "     Loose Maximum:      ( %3d    / %3d    / %3d    )\n",
                (unsigned int)n, Favg_r, Favg_g, Favg_b, Fst_r, Fst_g, Fst_b, min_r, min_g, min_b, max_r, max_g, max_b,
                Amin_r, Amin_g, Amin_b, Amax_r, Amax_g, Amax_b);
      else // if (bits_per_pixel <= 16)
        sprintf(text,
                "             Frame: %-8u (  Red  / Green / Blue  )\n"
                "           Average:      ( %7.2f / %7.2f / %7.2f )\n"
                "Standard Deviation:      ( %7.2f / %7.2f / %7.2f )\n"
                "           Minimum:      ( %5d    / %5d    / %5d    )\n"
                "           Maximum:      ( %5d    / %5d    / %5d    )\n"
                "     Loose Minimum:      ( %5d    / %5d    / %5d    )\n"
                "     Loose Maximum:      ( %5d    / %5d    / %5d    )\n",
                (unsigned int)n, Favg_r, Favg_g, Favg_b, Fst_r, Fst_g, Fst_b, min_r, min_g, min_b, max_r, max_g, max_b,
                Amin_r, Amin_g, Amin_b, Amax_r, Amax_g, Amax_b);
      env->ApplyMessage(&frame, vi, text, vi.width / 4, 0xa0a0a0, 0, 0);
    }
    env->Free(accum_r);
  }
  return frame;
}

AVSValue __cdecl RGBAdjust::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new RGBAdjust(args[0].AsClip(), args[1].AsDblDef(1.0), args[2].AsDblDef(1.0), args[3].AsDblDef(1.0),
                       args[4].AsDblDef(1.0), args[5].AsDblDef(0.0), args[6].AsDblDef(0.0), args[7].AsDblDef(0.0),
                       args[8].AsDblDef(0.0), args[9].AsDblDef(1.0), args[10].AsDblDef(1.0), args[11].AsDblDef(1.0),
                       args[12].AsDblDef(1.0), args[13].AsBool(false), args[14].AsBool(false), args[15].AsBool(false),
                       args[16].AsString(""), env);
}

/* helper function for Tweak and MaskHS filters */
