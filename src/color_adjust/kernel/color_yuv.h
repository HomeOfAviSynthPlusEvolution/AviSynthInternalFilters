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
static void coloryuv_showyuv(BYTE* pY, BYTE* pU, BYTE* pV, int y_pitch, int u_pitch, int v_pitch, int framenumber,
                             bool full_range, int bits_per_pixel) {
  int internal_bitdepth = bits_per_pixel == 8 ? 8 : 10;

  const int luma_min = full_range ? 0 : (16 << (internal_bitdepth - 8));
  const int luma_max = full_range ? (1 << internal_bitdepth) - 1 : (235 << (internal_bitdepth - 8));

  const int chroma_center = 128 << (internal_bitdepth - 8);
  const int chroma_span =
      full_range ? (1 << (internal_bitdepth - 1)) - 1 : (112 << (internal_bitdepth - 8)); // +-127/+-112
  const int chroma_min = chroma_center - chroma_span; // +-112 (16-240) -> +-127 (1-255)
  const int chroma_max = chroma_center + chroma_span;

  const int luma_range = luma_max - luma_min + 1; // 256/220 ,1024/880
  const int chroma_range = chroma_max - chroma_min + 1;

  const int luma_size = chroma_range * 2; // YUV output is always 4:2:0. Horizontal subsampling 2.

  int luma;
  // Calculate luma cycle
  // 0,1..255,254,..1 = 2x256-2
  // 0,1..1023,1022,..1 = 2*1024-2
  luma = framenumber % (luma_range * 2 - 2);
  if (luma > luma_range - 1)
    luma = (luma_range * 2 - 2) - luma;
  luma += luma_min;

  // Set luma value
  if (bits_per_pixel == 8) {
    for (int y = 0; y < luma_size; y++) {
      memset(pY, luma, luma_size);
      pY += y_pitch;
    }
  } else if (bits_per_pixel <= 16) {
    // display size (thus luma range) covers the same as at 10 bits.
    if (full_range) {
      // stretch
      const float factor = (float)((1 << bits_per_pixel) - 1) / ((1 << internal_bitdepth) - 1);
      const int luma_target = (int)(luma * factor + 0.5f);
      for (int y = 0; y < luma_size; y++) {
        std::fill_n((uint16_t*)pY, luma_size, luma_target);
        pY += y_pitch;
      }
    } else {
      // shift
      const int luma_target = luma << (bits_per_pixel - internal_bitdepth);
      for (int y = 0; y < luma_size; y++) {
        std::fill_n((uint16_t*)pY, luma_size, luma_target);
        pY += y_pitch;
      }
    }
  } else {
    // 32 bit float
    const float factor = 1.0f / ((1 << internal_bitdepth) - 1);
    const float luma_target = luma * factor;
    for (int y = 0; y < luma_size; y++) {
      std::fill_n((float*)pY, luma_size, luma_target);
      pY += y_pitch;
    }
  }

  // Set chroma
  if (full_range) {
    if (bits_per_pixel != 32) {
      // 8-16 bit full
      const int chroma_center_target = 128 << (bits_per_pixel - 8);
      const int chroma_span_target =
          full_range ? (1 << (bits_per_pixel - 1)) - 1 : (112 << (bits_per_pixel - 8)); // +-127/+-112
      const float factor = (float)chroma_span_target / chroma_span;
      const float chroma_center_target_plus_round = chroma_center_target + 0.5f;

      if (bits_per_pixel == 8) {
        for (int y = 0; y < chroma_range; y++) {
          for (int x = 0; x < chroma_range; x++) {
            int pixel_u = chroma_min + x;
            pixel_u = (int)((pixel_u - chroma_center) * factor + chroma_center_target_plus_round);
            pU[x] = pixel_u;
          }
          int pixel_v = chroma_min + y;
          pixel_v = (int)((pixel_v - chroma_center) * factor + chroma_center_target_plus_round);
          std::fill_n((uint8_t*)pV, chroma_range, pixel_v);

          pU += u_pitch;
          pV += v_pitch;
        }
      } else if (bits_per_pixel <= 16) {
        for (int y = 0; y < chroma_range; y++) {
          for (int x = 0; x < chroma_range; x++) {
            int pixel_u = chroma_min + x;
            pixel_u = (int)((pixel_u - chroma_center) * factor + chroma_center_target_plus_round);
            reinterpret_cast<uint16_t*>(pU)[x] = pixel_u;
          }
          int pixel_v = chroma_min + y;
          pixel_v = (int)((pixel_v - chroma_center) * factor + chroma_center_target_plus_round);
          std::fill_n((uint16_t*)pV, chroma_range, pixel_v);

          pU += u_pitch;
          pV += v_pitch;
        }
      }
    } else {
      // 32 bit float, full
      const float chroma_span_target = 0.5f; // +-0.5/+-112
      const float factor = chroma_span_target / chroma_span;

      for (int y = 0; y < chroma_range; y++) {
        for (int x = 0; x < chroma_range; x++) {
          const int pixel_u = chroma_min + x;
          const float pixel_u_f = (pixel_u - chroma_center) * factor;
          reinterpret_cast<float*>(pU)[x] = pixel_u_f;
        }

        const int pixel_v = chroma_min + y;
        const float pixel_v_f = (pixel_v - chroma_center) * factor;
        std::fill_n((float*)pV, chroma_range, pixel_v_f);

        pU += u_pitch;
        pV += v_pitch;
      }
    }
    // full end
  } else {
    if (bits_per_pixel == 8) {
      // 8 bit limited range
      for (int y = 0; y < chroma_range; y++) {
        for (int x = 0; x < chroma_range; x++)
          pU[x] = chroma_min + x;
        const int pixel_v = chroma_min + y;
        std::fill_n((uint8_t*)pV, chroma_range, pixel_v);
        pU += u_pitch;
        pV += v_pitch;
      }
    } else if (bits_per_pixel <= 16) {
      // 16 bit limited range
      const int bitdiff = (bits_per_pixel - internal_bitdepth);
      for (int y = 0; y < chroma_range; y++) {
        for (int x = 0; x < chroma_range; x++) {
          reinterpret_cast<uint16_t*>(pU)[x] = (chroma_min + x) << bitdiff;
        }
        const int pixel_v = (chroma_min + y) << bitdiff;
        std::fill_n((uint16_t*)pV, chroma_range, pixel_v);
        pU += u_pitch;
        pV += v_pitch;
      }
    } else {
      // 32 bit float limited
      const float factor = 1.0f / ((1 << internal_bitdepth) - 1);
      for (int y = 0; y < chroma_range; y++) {
        for (int x = 0; x < chroma_range; x++) {
          reinterpret_cast<float*>(pU)[x] = (float)(chroma_min + x - chroma_center) * factor;
        }
        const float pixel_v = (float)(chroma_min + y - chroma_center) * factor;
        std::fill_n((float*)pV, chroma_range, pixel_v);
        pU += u_pitch;
        pV += v_pitch;
      }
    }
  }
}

// luts are only for integer bits 8/10/12/14/16. float will be realtime
template <typename pixel_t>
static void coloryuv_create_lut(BYTE* lut8, const ColorYUVPlaneConfig* config, int bits_per_pixel,
                                bool tweaklike_params) {
  pixel_t* lut = reinterpret_cast<pixel_t*>(lut8);

  // scale is 256/1024/4096/16384/65536
  // normalize to [0..1) working range
  const double value_normalization_scale = (double)((int64_t)1 << bits_per_pixel);
  // For gamma pre-post correction.
  const double tv_range_lo_normalized = 16.0 / 256.0;

  const int lookup_size = 1 << bits_per_pixel; // 256, 1024, 4096, 16384, 65536
  const int source_max = (1 << bits_per_pixel) - 1;
  const bool chroma = config->plane == PLANAR_U || config->plane == PLANAR_V;
  const bool fulls = config->range == COLORYUV_RANGE_PC_TV;
  const bool fulld = config->range == COLORYUV_RANGE_TV_PC;
  // when COLORYUV_RANGE_NONE both are the same false

  //-----------------------
  bits_conv_constants d;
  // When calculating src_pixel, src and dst are of the same bit depth
  get_bits_conv_constants(d, chroma, fulls, fulld, bits_per_pixel, bits_per_pixel);

  auto dst_offset_plus_round = d.dst_offset + 0.5;
  const int src_pixel_min = 0;
  const int src_pixel_max = source_max;

  // parameters are not scaled by bitdepth (legacy 8 bit behaviour)
  double gain = tweaklike_params ? config->gain : (config->gain / 256 + 1.0);
  double contrast = tweaklike_params ? config->contrast : (config->contrast / 256 + 1.0);
  double gamma = tweaklike_params ? config->gamma : (config->gamma / 256 + 1.0);
  double offset = config->offset / 256;

  // for correct Y gamma
  double range_factor_tv_to_pc = 255.0 / 219.0; // 16-235 ==> 0..255
  double range_factor_pc_to_tv = 219.0 / 255.0; // 0..255 ==> 16-235 (219)

  // for coring
  const int tv_range_lo_luma_chroma = (16 << (bits_per_pixel - 8));
  const int tv_range_hi_luma = (235 << (bits_per_pixel - 8));
  const int tv_range_hi_chroma = (240 << (bits_per_pixel - 8));

  // We know that the input is TV range for sure: (coring=true and !PC->TV), levels="TV->PC" or (new!) levels="TV"
  const bool source_is_limited = (config->clip_tv && config->range != COLORYUV_RANGE_PC_TV) ||
                                 config->range == COLORYUV_RANGE_TV_PC || config->force_tv_range;

  for (int i = 0; i < lookup_size; i++) {
    double value = double(i);
    value /= value_normalization_scale; // normalize to [0..1). For chroma this makes the center to 0.5.
    value *= gain;                      // Applying gain
    // Applying contrast. For chroma, this sets saturation
    value = (value - 0.5) * contrast + 0.5; // integer chroma center is transformed to 0, apply contrast
    // in Classic AVS: constract is applied on the original value and not on the already gained value
    // value = (value * gain) + ((value - 0.5) * contrast + 0.5) - value + (bright - 1);
    value += offset; // Applying offset

    // Applying gamma. Only on Y
    if (gamma != 0) {
      if (source_is_limited) {
        // avs+ 180301- use gamma on the proper 0.0 based value
        if (value > tv_range_lo_normalized) {
          // tv->pc
          value = (value - tv_range_lo_normalized) * range_factor_tv_to_pc; // (v-16)*range
          value = pow(value, 1.0 / gamma);
          // pc->tv
          value = value * range_factor_pc_to_tv + tv_range_lo_normalized; // v*range - 16
        }
      } else { // full (PC) range
        if (value > 0)
          value = pow(value, 1.0 / gamma);
      }
    }

    value *= value_normalization_scale; // back from [0..1) range

    if (fulls != fulld)
      // Range conversion
      value = (value - d.src_offset) * d.mul_factor + dst_offset_plus_round;
    else
      value = value + 0.5; // rounder

    // back to the integer world
    int iValue = clamp((int)value, src_pixel_min, src_pixel_max);

    if (config->clip_tv) // set when coring
    {
      iValue =
          clamp(iValue, tv_range_lo_luma_chroma, config->plane == PLANAR_Y ? tv_range_hi_luma : tv_range_hi_chroma);
    }

    lut[i] = iValue;
  }
}

// works with <= 16 bits, but used only for float
static std::string coloryuv_create_lut_expr(const ColorYUVPlaneConfig* config, int bits_per_pixel,
                                            bool tweaklike_params) {
  const bool f32 = bits_per_pixel == 32;
  const bool chroma = config->plane == PLANAR_U || config->plane == PLANAR_V;
  // 32 bit float is already in [0..1] range but we have to make it similar to integer [0..1): needs /256 and not /255.
  // Reason: match to the integer behaviour
  // integer: normalize to [0..1) working range
  const double value_normalization_scale = f32 ? (256.0 / 255.0) : (double)((int64_t)1 << bits_per_pixel);
  // For gamma pre-post correction. we are in [0..1) working range
  const double tv_range_lo_normalized = 16.0 / 256.0;

  //const double source_max = f32 ? 1.0 : (double)((1 << bits_per_pixel) - 1);
  const bool fulls = config->range == COLORYUV_RANGE_PC_TV;
  const bool fulld = config->range == COLORYUV_RANGE_TV_PC;
  // when COLORYUV_RANGE_NONE both are the same false

  bits_conv_constants d;
  // When calculating src_pixel, src and dst are of the same bit depth
  get_bits_conv_constants(d, chroma, fulls, fulld, bits_per_pixel, bits_per_pixel);

  auto dst_offset_no_round = d.dst_offset;
  //const auto src_pixel_max = source_max;

  // parameters are not scaled by bitdepth (legacy 8 bit behaviour)
  double gain = tweaklike_params ? config->gain : (config->gain / 256 + 1.0);
  double contrast = tweaklike_params ? config->contrast : (config->contrast / 256 + 1.0);
  double gamma = tweaklike_params ? config->gamma : (config->gamma / 256 + 1.0);
  double offset = config->offset / 256; // always in the 256 range

  // for correct Y gamma
  double range_factor_tv_to_pc = 255.0 / 219.0; // 16-235 ==> 0..255
  double range_factor_pc_to_tv = 219.0 / 255.0; // 0..255 ==> 16-235 (219)

  // We know that the input is TV range for sure: (coring=true and !PC->TV), levels="TV->PC" or (new!) levels="TV"
  const bool source_is_limited = (config->clip_tv && config->range != COLORYUV_RANGE_PC_TV) ||
                                 config->range == COLORYUV_RANGE_TV_PC || config->force_tv_range;

  std::stringstream ss;

  // value = double(i), we are using floats here
  ss << "x";

  // value = value / value_normalization_scale;
  if (chroma && f32)
    ss << " 255 * 256 / 0.5 + "; // from float chroma +/- to match with legacy integer behaviour
  else
    ss << " " << value_normalization_scale << " /";

  if (gain != 1.0)
    ss << " " << gain << " *"; // Applying gain // value *= gain;

  // Applying contrast. For chroma, this sets saturation
  //  value = (value - 0.5) * contrast + 0.5;
  // earlier we made measures that float chroma center is kept at 0.5
  ss << " 0.5 - " << contrast << " * 0.5 +";

  // value += offset; // Applying offset
  ss << " " << offset << " +";

  // Applying gamma. Only on Y
  if (gamma != 0) {
    if (source_is_limited) {
      // avs+ 180301- use gamma on the proper 0.0 based value
      // value = value > 16scaled ? (pow((value - 16scl)*range_tv_pc,(1/gamma))*range_pc_tv+16d : value
      ss << " A@ " << tv_range_lo_normalized
         << " > " // condition: value > tv_range_lo_normalized
         // case: true. tv->pc, power, pc->tv
         << "A " << tv_range_lo_normalized << " - " << range_factor_tv_to_pc << " * " << (1.0 / gamma) << " pow "
         << range_factor_pc_to_tv << " * " << tv_range_lo_normalized
         << " + "
         // case: false. Original value
         << " A ? ";
    } else { // full (PC) range
      //if (value > 0)
      //  value = pow(value, 1.0 / gamma);
      // value = value > 0 ? pow(value,(1/gamma)) : value
      ss << " A@ 0 > A " << (1.0 / gamma) << " pow A ? ";
    }
  }

  if (chroma && f32)
    ss << " 0.5 - 256 * 255 / ";
  else
    ss << " " << value_normalization_scale << " * "; // value *= value_normalization_scale; // back from [0..1) range

  if (fulls != fulld) {
    ss << d.src_offset << " - " << d.mul_factor << " * " << dst_offset_no_round << " + ";
    // value = (value - d.src_offset) * d.mul_factor + dst_offset_no_round; // no rounder, Expr will round automatically before return
  } else {
    // value = value + 0.5; // no rounder, Expr rounds
  }

  // clamp to the original valid bitdepth is done by Expr
  // int iValue = clamp((int)value, src_pixel_min, src_pixel_max);
  // ss << " " << src_pixel_min << " max " << src_pixel_max << " min ";

  if (config->clip_tv) // set when coring
  {
    double tv_range_lo_luma = f32 ? (16.0 / 255) : ((int64_t)16 << (bits_per_pixel - 8));
    double tv_range_hi_luma = f32 ? (235.0 / 255) : ((int64_t)235 << (bits_per_pixel - 8));
    double tv_range_lo_chroma =
        f32 ? (-112.0 / 255.0)
            : ((int64_t)16 << (bits_per_pixel - 8)); // 112/255.0 consistent with get_bits_conv_constants()
    double tv_range_hi_chroma = f32 ? (+112.0 / 255.0) : ((int64_t)240 << (bits_per_pixel - 8));
    ss << (config->plane == PLANAR_Y ? tv_range_lo_luma : tv_range_lo_chroma) << " max ";
    ss << (config->plane == PLANAR_Y ? tv_range_hi_luma : tv_range_hi_chroma) << " min ";
    //iValue = clamp(iValue, tv_range_lo_luma, config->plane == PLANAR_Y ? tv_range_hi_luma : tv_range_hi_chroma);
  }

  std::string exprString = ss.str();
  return exprString;
}

// for float, only loose_min and loose_max is counted
template <bool forFloat>
static void coloryuv_analyse_core(const int* freq, const int pixel_num, ColorYUVPlaneData* data,
                                  int bits_per_pixel_for_stat) {
  // 32 bit float reached here as split into ranges like a 16bit clip
  int pixel_value_count = 1 << bits_per_pixel_for_stat; // size of freq table

  const int pixel_256th = pixel_num / 256; // For loose max/min yes, still 1/256!

  double avg = 0.0;
  int real_min, real_max;

  if (!forFloat) {
    real_min = -1;
    real_max = -1;
  }
  data->loose_max = -1;
  data->loose_min = -1;

  int px_min_c = 0, px_max_c = 0;

  for (int i = 0; i < pixel_value_count; i++) {
    if (!forFloat) {
      avg += freq[i] * double(i);

      if (freq[i] > 0 && real_min == -1) {
        real_min = i;
      }
    }

    if (data->loose_min == -1) {
      px_min_c += freq[i];

      if (px_min_c > pixel_256th) {
        data->loose_min = i;
      }
    }

    if (!forFloat) {
      if (freq[pixel_value_count - 1 - i] > 0 && real_max == -1) {
        real_max = pixel_value_count - 1 - i;
      }
    }

    if (data->loose_max == -1) {
      px_max_c += freq[pixel_value_count - 1 - i];

      if (px_max_c > pixel_256th) {
        data->loose_max = pixel_value_count - 1 - i;
      }
    }
  }

  if (!forFloat) {
    avg /= pixel_num;
    data->average = avg;
    data->real_min = (float)real_min;
    data->real_max = (float)real_max;
  }
}

static void coloryuv_analyse_planar(const BYTE* pSrc, int src_pitch, int width, int height, ColorYUVPlaneData* data,
                                    int bits_per_pixel, bool chroma) {
  // We can gather statistics from float, but for population count we have to
  // split the range. We decide to split it into 2^16 ranges.
  int bits_per_pixel_for_freq = bits_per_pixel <= 16 ? bits_per_pixel : 16;
  int statistics_size = 1 << bits_per_pixel_for_freq; // float: 65536
  int* freq = new int[statistics_size];
  std::fill_n(freq, statistics_size, 0);

  double sum = 0.0; // for float
  float real_min;
  float real_max;

  if (bits_per_pixel == 8) {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        freq[pSrc[x]]++;
      }

      pSrc += src_pitch;
    }
  } else if (bits_per_pixel >= 10 && bits_per_pixel <= 14) {
    uint16_t mask = statistics_size - 1; // e.g. 0x3FF for 10 bit
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        freq[clamp(reinterpret_cast<const uint16_t*>(pSrc)[x], (uint16_t)0, mask)]++;
      }

      pSrc += src_pitch;
    }
  } else if (bits_per_pixel == 16) {
    // no clamp, faster
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        freq[reinterpret_cast<const uint16_t*>(pSrc)[x]]++;
      }

      pSrc += src_pitch;
    }
  } else if (bits_per_pixel == 32) {
    // 32 bits: we populate pixels only for loose_min and loose_max
    // real_min and real_max, average (sum) is computed differently
    real_min = reinterpret_cast<const float*>(pSrc)[0];
    real_max = real_min;
    if (chroma) {
      const float shift = 32768.0f;
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
          // -0.5..0.5 (0..1.0 when FLOAT_CHROMA_IS_HALF_CENTERED) to 0..65535
          // see also: ConditionalFunctions MinMax
          const float pixel = reinterpret_cast<const float*>(pSrc)[x];
          freq[clamp((int)(65535.0f * pixel + shift + 0.5f), 0, 65535)]++;
          // todo: SSE2
          real_min = min(real_min, pixel);
          real_max = max(real_max, pixel);
          sum += pixel;
        }
        pSrc += src_pitch;
      }
    } else {
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
          // 0..1 -> 0..65535
          const float pixel = reinterpret_cast<const float*>(pSrc)[x];
          freq[clamp((int)(65535.0f * pixel + 0.5f), 0, 65535)]++;
          // todo: SSE2
          real_min = min(real_min, pixel);
          real_max = max(real_max, pixel);
          sum += pixel;
        }

        pSrc += src_pitch;
      }
    }
  }

  if (bits_per_pixel == 32) {
    coloryuv_analyse_core<true>(freq, width * height, data, bits_per_pixel_for_freq);
    data->average = sum / (height * width);
    data->real_max = real_max;
    data->real_min = real_min;
    // loose min and max was shifted by half of 16bit range. We still keep here the range
    if (chroma) {
      data->loose_max = data->loose_max - 32768;
      data->loose_min = data->loose_min - 32768;
    }
    // autogain treats it as a value of 16bit magnitude, show=true as well
    //data->loose_min = data->loose_min / 65535.0f; not now.
    //data->loose_max = data->loose_max / 65535.0f;
  } else {
    coloryuv_analyse_core<false>(freq, width * height, data, bits_per_pixel_for_freq);
  }

  delete[] freq;
}

static void coloryuv_analyse_yuy2(const BYTE* pSrc, int src_pitch, int width, int height, ColorYUVPlaneData* dataY,
                                  ColorYUVPlaneData* dataU, ColorYUVPlaneData* dataV) {
  int freqY[256], freqU[256], freqV[256];
  memset(freqY, 0, sizeof(freqY));
  memset(freqU, 0, sizeof(freqU));
  memset(freqV, 0, sizeof(freqV));

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width * 2; x += 4) {
      freqY[pSrc[x + 0]]++;
      freqU[pSrc[x + 1]]++;
      freqY[pSrc[x + 2]]++;
      freqV[pSrc[x + 3]]++;
    }

    pSrc += src_pitch;
  }

  coloryuv_analyse_core<false>(freqY, width * height, dataY, 8);
  coloryuv_analyse_core<false>(freqU, width * height / 2, dataU, 8);
  coloryuv_analyse_core<false>(freqV, width * height / 2, dataV, 8);
}

static void coloryuv_autogain(const ColorYUVPlaneData* dY, const ColorYUVPlaneData* /*dU*/,
                              const ColorYUVPlaneData* /*dV*/, ColorYUVPlaneConfig* cY, ColorYUVPlaneConfig* /*cU*/,
                              ColorYUVPlaneConfig* /*cV*/, int bits_per_pixel, bool tweaklike_params) {
  int bits_per_pixel_for_freq =
      bits_per_pixel <= 16 ? bits_per_pixel : 16; // for float: "loose" statistics like uint16_t
  // always 16..235
  int loose_max_limit = (235 + 1) << (bits_per_pixel_for_freq - 8);
  int loose_min_limit = 16 << (bits_per_pixel_for_freq - 8);
  int maxY = min(dY->loose_max, loose_max_limit);
  int minY = max(dY->loose_min, loose_min_limit);

  int range = maxY - minY;

  if (range > 0) {
    double scale = double(loose_max_limit - loose_min_limit) / range;
    cY->offset =
        (loose_min_limit - scale * minY) / (1 << (bits_per_pixel_for_freq - 8)); // good for float also, 0..256 range
    cY->gain = tweaklike_params ? scale : (256 * (scale - 1.0));
    cY->changed = true;
  }
}

static void coloryuv_autowhite(const ColorYUVPlaneData* /*dY*/, const ColorYUVPlaneData* dU,
                               const ColorYUVPlaneData* dV, ColorYUVPlaneConfig* /*cY*/, ColorYUVPlaneConfig* cU,
                               ColorYUVPlaneConfig* cV, int bits_per_pixel) {
  if (bits_per_pixel == 32) {
#ifdef FLOAT_CHROMA_IS_HALF_CENTERED
    double middle = 0.5;
#else
    double middle = 0.0;
#endif
    cU->offset = (middle - dU->average) * 256; // parameter is in 256 range
    cV->offset = (middle - dV->average) * 256;
  } else {
    double middle = (1 << (bits_per_pixel - 1)) - 1;                   // 128-1, 2048-1 ...
    cU->offset = (middle - dU->average) / (1 << (bits_per_pixel - 8)); // parameter is in 256 range
    cV->offset = (middle - dV->average) / (1 << (bits_per_pixel - 8));
  }
  cU->changed = true;
  cV->changed = true;
}

// only for integer samples
static void coloryuv_apply_lut_planar(BYTE* dst, const BYTE* src, int dp, int sp, int w, int h, const BYTE* lut,
                                      int bits, IScriptEnvironment* env, uint32_t cpu_mask) {
  map_channel(dst, dp, src, sp, w, h, lut, bits, 1, env, cpu_mask);
}
static void coloryuv_apply_lut_yuy2(BYTE* dst, const BYTE* src, int dp, int sp, int w, int h, const BYTE* y,
                                    const BYTE* u, const BYTE* v, IScriptEnvironment* env, uint32_t cpu_mask) {
  map_channel(dst, dp, src, sp, w, h, y, 8, 2, env, cpu_mask);
  map_channel(dst + 1, dp, src + 1, sp, w / 2, h, u, 8, 4, env, cpu_mask);
  map_channel(dst + 3, dp, src + 3, sp, w / 2, h, v, 8, 4, env, cpu_mask);
}
