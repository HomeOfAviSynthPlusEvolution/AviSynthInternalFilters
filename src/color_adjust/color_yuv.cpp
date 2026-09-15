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

#include "color_yuv.h"
#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <cstdio>
#include <avs/minmax.h>
#include <avs/alignment.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <string>
#include "color_properties.h"
#include "kernel/range.h"
#include "kernel_adapter.h"
#include "kernel/color_yuv.h"
#define READ_CONDITIONAL(plane, var_name, internal_name, condVarSuffix)                                                \
  {                                                                                                                    \
    std::string s = "coloryuv_" #var_name "_" #plane;                                                                  \
    s = s + condVarSuffix;                                                                                             \
    const double t = env->GetVarDouble(s.c_str(), DBL_MIN);                                                            \
    if (t != DBL_MIN) {                                                                                                \
      c_##plane->internal_name = t;                                                                                    \
      c_##plane->changed = true;                                                                                       \
    }                                                                                                                  \
  }

// extra: add extra at the end of variable names: different variables for multiple instances of coloryuv
static void coloryuv_read_conditional(IScriptEnvironment* env, ColorYUVPlaneConfig* c_y, ColorYUVPlaneConfig* c_u,
                                      ColorYUVPlaneConfig* c_v, const char* condVarSuffix) {
  READ_CONDITIONAL(y, gain, gain, condVarSuffix);
  READ_CONDITIONAL(y, off, offset, condVarSuffix);
  READ_CONDITIONAL(y, gamma, gamma, condVarSuffix);
  READ_CONDITIONAL(y, cont, contrast, condVarSuffix);

  READ_CONDITIONAL(u, gain, gain, condVarSuffix);
  READ_CONDITIONAL(u, off, offset, condVarSuffix);
  READ_CONDITIONAL(u, gamma, gamma, condVarSuffix);
  READ_CONDITIONAL(u, cont, contrast, condVarSuffix);

  READ_CONDITIONAL(v, gain, gain, condVarSuffix);
  READ_CONDITIONAL(v, off, offset, condVarSuffix);
  READ_CONDITIONAL(v, gamma, gamma, condVarSuffix);
  READ_CONDITIONAL(v, cont, contrast, condVarSuffix);
}

#undef READ_CONDITIONAL

ColorYUV::ColorYUV(PClip child, double gain_y, double offset_y, double gamma_y, double contrast_y, double gain_u,
                   double offset_u, double gamma_u, double contrast_u, double gain_v, double offset_v, double gamma_v,
                   double contrast_v, const char* level, const char* opt, bool showyuv, bool analyse, bool autowhite,
                   bool autogain, bool conditional, int bits, bool showyuv_fullrange,
                   bool tweaklike_params, // ColorYUV2: 0.0/0.5/1.0/2.0/3.0 instead of -256/-128/0/256/512
                   const char* condVarSuffix, bool optForceUseExpr, IScriptEnvironment* env)
    : GenericVideoFilter(child), cpu_mask_(color_cpu(env)), colorbar_bits(showyuv ? bits : 0),
      colorbar_fullrange(showyuv_fullrange), analyse(analyse), autowhite(autowhite), autogain(autogain),
      conditional(conditional), tweaklike_params(tweaklike_params), condVarSuffix(condVarSuffix),
      optForceUseExpr(optForceUseExpr) {
  luts[0] = luts[1] = luts[2] = nullptr;

  if (!vi.IsYUV() && !vi.IsYUVA())
    env->ThrowError("ColorYUV: Only work with YUV colorspace.");

  bool ColorRangeCanBeGuessed = false;
  if (gamma_y != 0) {
    ColorRangeCanBeGuessed = true;
    // when gamma is used then we must know the color range full/limited
    // and the default is full_scale if gamma is not zero
  }

  configY.gain = gain_y;
  configY.offset = offset_y;
  configY.gamma = gamma_y;
  configY.contrast = contrast_y;
  configY.changed = false;
  configY.clip_tv = false;
  configY.force_tv_range = false;
  configY.plane = PLANAR_Y;

  configU.gain = gain_u;
  configU.offset = offset_u;
  configU.gamma = gamma_u;
  configU.contrast = contrast_u;
  configU.changed = false;
  configU.clip_tv = false;
  configU.force_tv_range = false; // n/a. in chroma. For gamma
  configU.plane = PLANAR_U;

  configV.gain = gain_v;
  configV.offset = offset_v;
  configV.gamma = gamma_v;
  configV.contrast = contrast_v;
  configV.changed = false;
  configV.clip_tv = false;
  configV.force_tv_range = false; // n/a. in chroma. For gamma
  configV.plane = PLANAR_V;

  // Range
  if (coloryuv_stricmp(level, "TV->PC") == 0) {
    ColorRangeCanBeGuessed = true; // will be full
    configV.range = configU.range = configY.range = COLORYUV_RANGE_TV_PC;
  } else if (coloryuv_stricmp(level, "PC->TV") == 0) {
    ColorRangeCanBeGuessed = true; // will be limited
    configV.range = configU.range = configY.range = COLORYUV_RANGE_PC_TV;
  } else if (coloryuv_stricmp(level, "PC->TV.Y") == 0) { // ?
    ColorRangeCanBeGuessed = true;                       // will be limited
    configV.range = configU.range = COLORYUV_RANGE_NONE;
    configY.range = COLORYUV_RANGE_PC_TV;
  } else if (coloryuv_stricmp(level, "TV") == 0) {
    // When no range conversion occurs only gamma correction
    // By this parameter we know it will be limited, this info is used for gamma adjustment
    ColorRangeCanBeGuessed = true;
    configV.force_tv_range = configU.force_tv_range = configY.force_tv_range = true;
  } else if (coloryuv_stricmp(level, "") != 0) {
    env->ThrowError("ColorYUV: invalid parameter : levels");
  } else {
    configV.range = configU.range = configY.range = COLORYUV_RANGE_NONE;
  }

  // Option
  if (coloryuv_stricmp(opt, "coring") == 0) {
    // note: this setting can conflict with e.g. TV->PC but we do not report an error
    ColorRangeCanBeGuessed = true; // used to set _ColorRange=limited only if not conversion mode is specified
    configY.clip_tv = true;
    configU.clip_tv = true;
    configV.clip_tv = true;
  } else if (coloryuv_stricmp(opt, "") != 0) {
    env->ThrowError("ColorYUV: invalid parameter : opt");
  }

  if (showyuv) {
    if (colorbar_bits != 8 && colorbar_bits != 10 && colorbar_bits != 12 && colorbar_bits != 14 &&
        colorbar_bits != 16 && colorbar_bits != 32)
      env->ThrowError("ColorYUV: bits parameter for showyuv must be 8, 10, 12, 14, 16 or 32");

    switch (colorbar_bits) {
      case 8:
        vi.pixel_type = VideoInfo::CS_YV12;
        break;
      case 10:
        vi.pixel_type = VideoInfo::CS_YUV420P10;
        break;
      case 12:
        vi.pixel_type = VideoInfo::CS_YUV420P12;
        break;
      case 14:
        vi.pixel_type = VideoInfo::CS_YUV420P14;
        break;
      case 16:
        vi.pixel_type = VideoInfo::CS_YUV420P16;
        break;
      case 32:
        vi.pixel_type = VideoInfo::CS_YUV420PS;
        break;
    }
    // pre-avs+: coloryuv_showyuv is always called with full_range false
    const int internal_bitdepth = colorbar_bits == 8 ? 8 : 10;
    const int chroma_span =
        colorbar_fullrange ? (1 << (internal_bitdepth - 1)) - 1 : (112 << (internal_bitdepth - 8)); // +-127/+-112
    const int chroma_range = 2 * chroma_span + 1; // 1..255 (+-127), 16..240 (+-112)
    // size limited to either 8 or 10 bits, independenly of 12/14/16 or 32bit float bit-depth
    vi.width = chroma_range << vi.GetPlaneWidthSubsampling(PLANAR_U);
    vi.height = vi.width;
    theColorRange = colorbar_fullrange ? ColorRange_e::AVS_RANGE_FULL : ColorRange_e::AVS_RANGE_LIMITED;
    theMatrix = Matrix_e::AVS_MATRIX_BT470_BG;               // all the same
    theChromaLocation = ChromaLocation_e::AVS_CHROMA_CENTER; // default "mpeg1" for 4:2:0
    return;
  }

  // !showyuv, real filter

  auto frame0 = child->GetFrame(0, env);
  const AVSMap* props = env->getFramePropsRO(frame0);
  read_color_yuv_properties(props, theMatrix, theColorRange, theOutColorRange,
                            configY.force_tv_range   ? ColorRange_e::AVS_RANGE_LIMITED
                            : ColorRangeCanBeGuessed ? ColorRange_e::AVS_RANGE_FULL
                                                     : -1,
                            env);
  // although we read _ColorRange full/limited, nothing stops us to feed with full-range clip a "TV->PC" conversion
  // Anyway: a frame property can set theColorRange from the default "FULL" to the actual one.
  switch (configY.range) {
    case COLORYUV_RANGE_PC_TV:
    case COLORYUV_RANGE_PC_TVY:
      theColorRange = ColorRange_e::AVS_RANGE_LIMITED;
      break;
    case COLORYUV_RANGE_TV_PC:
      theColorRange = ColorRange_e::AVS_RANGE_FULL;
      break;
    default:
      if (configY.force_tv_range) // "TV" overrides default "PC". Info is needed for gamma correction
        theColorRange = ColorRange_e::AVS_RANGE_LIMITED;
      else if (configY.clip_tv) // coring is also sets this frame property
        theColorRange = ColorRange_e::AVS_RANGE_LIMITED;
      else
        theColorRange = theOutColorRange;
      break;
      // leave color range as is
  }
  // theMatrix and theColorRange will set frame properties in GetFrame

  // prepare basic LUT
  int pixelsize = vi.ComponentSize();
  int bits_per_pixel = vi.BitsPerComponent();

  if (pixelsize == 1 || pixelsize == 2) {
    // no float lut. float will be realtime
    int lut_size = pixelsize * (1 << bits_per_pixel); // 256*1 / 1024*2 .. 65536*2
    luts[0] = new BYTE[lut_size];
    if (!vi.IsY()) {
      luts[1] = new BYTE[lut_size];
      luts[2] = new BYTE[lut_size];
    }

    if (pixelsize == 1) {
      coloryuv_create_lut<uint8_t>(luts[0], &configY, bits_per_pixel, tweaklike_params);
      if (!vi.IsY()) {
        coloryuv_create_lut<uint8_t>(luts[1], &configU, bits_per_pixel, tweaklike_params);
        coloryuv_create_lut<uint8_t>(luts[2], &configV, bits_per_pixel, tweaklike_params);
      }
    } else if (pixelsize == 2) { // pixelsize==2
      coloryuv_create_lut<uint16_t>(luts[0], &configY, bits_per_pixel, tweaklike_params);
      if (!vi.IsY()) {
        coloryuv_create_lut<uint16_t>(luts[1], &configU, bits_per_pixel, tweaklike_params);
        coloryuv_create_lut<uint16_t>(luts[2], &configV, bits_per_pixel, tweaklike_params);
      }
    }
  }
}

ColorYUV::~ColorYUV() {
  if (luts[0])
    delete[] luts[0];
  if (luts[1])
    delete[] luts[1];
  if (luts[2])
    delete[] luts[2];
}

PVideoFrame __stdcall ColorYUV::GetFrame(int n, IScriptEnvironment* env) {
  if (colorbar_bits > 0) {
    PVideoFrame dst = env->NewVideoFrame(vi);
    // no frame property source. It's like a source filter

    auto props = env->getFramePropsRW(dst);
    update_color_yuv_matrix_and_range(props, theMatrix, theColorRange, env);
    update_color_yuv_chroma_location(props, theChromaLocation, env);

    // pre AVS+: full_range is always false
    // AVS+: showyuv_fullrange bool parameter
    // AVS+: bits parameter
    coloryuv_showyuv(dst->GetWritePtr(), dst->GetWritePtr(PLANAR_U), dst->GetWritePtr(PLANAR_V), dst->GetPitch(),
                     dst->GetPitch(PLANAR_U), dst->GetPitch(PLANAR_V), n, colorbar_fullrange, colorbar_bits);
    return dst;
  }

  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst;

  int pixelsize = vi.ComponentSize();
  int bits_per_pixel = vi.BitsPerComponent();

  ColorYUVPlaneConfig // Yes, we copy these struct
      cY = configY,
      cU = configU, cV = configV;

  // for analysing data
  char text[512];

  if (analyse || autowhite || autogain) {
    ColorYUVPlaneData dY, dU, dV;

    if (vi.IsYUY2()) {
      coloryuv_analyse_yuy2(src->GetReadPtr(), src->GetPitch(), vi.width, vi.height, &dY, &dU, &dV);
    } else {
      coloryuv_analyse_planar(src->GetReadPtr(), src->GetPitch(), vi.width, vi.height, &dY, bits_per_pixel,
                              false); // false: not chroma
      if (!vi.IsY()) {
        const int width = vi.width >> vi.GetPlaneWidthSubsampling(PLANAR_U);
        const int height = vi.height >> vi.GetPlaneHeightSubsampling(PLANAR_U);

        coloryuv_analyse_planar(src->GetReadPtr(PLANAR_U), src->GetPitch(PLANAR_U), width, height, &dU, bits_per_pixel,
                                true); // true: chroma
        coloryuv_analyse_planar(src->GetReadPtr(PLANAR_V), src->GetPitch(PLANAR_V), width, height, &dV, bits_per_pixel,
                                true);
      }
    }

    if (analyse) {
      if (!vi.IsY()) {
        if (bits_per_pixel == 32)
          sprintf(text,
                  "        Frame: %-8u ( Luma Y / ChromaU / ChromaV )\n"
                  "      Average:      ( %7.5f / %7.5f / %7.5f )\n"
                  "      Minimum:      ( %7.5f / %7.5f / %7.5f )\n"
                  "      Maximum:      ( %7.5f / %7.5f / %7.5f )\n"
                  "Loose Minimum:      ( %7.5f / %7.5f / %7.5f )\n"
                  "Loose Maximum:      ( %7.5f / %7.5f / %7.5f )\n",
                  n, dY.average, dU.average, dV.average, dY.real_min, dU.real_min, dV.real_min, dY.real_max,
                  dU.real_max, dV.real_max, (float)dY.loose_min / 65535.0f, (float)dU.loose_min / 65535.0f,
                  (float)dV.loose_min / 65535.0f, (float)dY.loose_max / 65535.0f, (float)dU.loose_max / 65535.0f,
                  (float)dV.loose_max / 65535.0f);
        else
          sprintf(text,
                  "        Frame: %-8u ( Luma Y / ChromaU / ChromaV )\n"
                  "      Average:      ( %7.2f / %7.2f / %7.2f )\n"
                  "      Minimum:      (  %5d  /  %5d  /  %5d   )\n"
                  "      Maximum:      (  %5d  /  %5d  /  %5d   )\n"
                  "Loose Minimum:      (  %5d  /  %5d  /  %5d   )\n"
                  "Loose Maximum:      (  %5d  /  %5d  /  %5d   )\n",
                  n, dY.average, dU.average, dV.average, (int)dY.real_min, (int)dU.real_min, (int)dV.real_min,
                  (int)dY.real_max, (int)dU.real_max, (int)dV.real_max, dY.loose_min, dU.loose_min, dV.loose_min,
                  dY.loose_max, dU.loose_max, dV.loose_max);
      } else {
        if (bits_per_pixel == 32)
          sprintf(text,
                  "        Frame: %-8u\n"
                  "      Average: %7.5f\n"
                  "      Minimum: %7.5f\n"
                  "      Maximum: %7.5f\n"
                  "Loose Minimum: %7.5f\n"
                  "Loose Maximum: %7.5f\n",
                  n, dY.average, dY.real_min, dY.real_max, (float)dY.loose_min / 65535.0f,
                  (float)dY.loose_max / 65535.0f);
        else
          sprintf(text,
                  "        Frame: %-8u\n"
                  "      Average: %7.2f\n"
                  "      Minimum: %5d\n"
                  "      Maximum: %5d\n"
                  "Loose Minimum: %5d\n"
                  "Loose Maximum: %5d\n",
                  n, dY.average, (int)dY.real_min, (int)dY.real_max, dY.loose_min, dY.loose_max);
      }
    }

    if (autowhite && !vi.IsY()) {
      coloryuv_autowhite(&dY, &dU, &dV, &cY, &cU, &cV, bits_per_pixel);
    }

    if (autogain) {
      coloryuv_autogain(&dY, &dU, &dV, &cY, &cU, &cV, bits_per_pixel, tweaklike_params);
    }
  }

  // Read conditional variables
  if (conditional)
    coloryuv_read_conditional(env, &cY, &cU, &cV, condVarSuffix);

  // no float lut. float will be realtime
  if ((pixelsize == 1 || pixelsize == 2) && !optForceUseExpr) {

    BYTE* luts_live[3] = {nullptr};

    BYTE* lutY = luts[0];
    BYTE* lutU = luts[1];
    BYTE* lutV = luts[2];

    // recalculate plane LUT only if changed
    int lut_size = pixelsize * (1 << bits_per_pixel); // 256*1 / 1024*2 .. 65536*2
    if (cY.changed) {
      luts_live[0] = new BYTE[lut_size];
      lutY = luts_live[0];
      if (pixelsize == 1)
        coloryuv_create_lut<uint8_t>(lutY, &cY, bits_per_pixel, tweaklike_params);
      else if (pixelsize == 2)
        coloryuv_create_lut<uint16_t>(lutY, &cY, bits_per_pixel, tweaklike_params);
    }

    if (!vi.IsY()) {
      if (cU.changed) {
        luts_live[1] = new BYTE[lut_size];
        lutU = luts_live[1];
        if (pixelsize == 1)
          coloryuv_create_lut<uint8_t>(lutU, &cU, bits_per_pixel, tweaklike_params);
        else if (pixelsize == 2)
          coloryuv_create_lut<uint16_t>(lutU, &cU, bits_per_pixel, tweaklike_params);
      }
      if (cV.changed) {
        luts_live[2] = new BYTE[lut_size];
        lutV = luts_live[2];
        if (pixelsize == 1)
          coloryuv_create_lut<uint8_t>(lutV, &cV, bits_per_pixel, tweaklike_params);
        else if (pixelsize == 2)
          coloryuv_create_lut<uint16_t>(lutV, &cV, bits_per_pixel, tweaklike_params);
      }
    }
    dst = env->NewVideoFrameP(vi, &src);

    if (vi.IsYUY2()) {
      coloryuv_apply_lut_yuy2(dst->GetWritePtr(), src->GetReadPtr(), dst->GetPitch(), src->GetPitch(), vi.width,
                              vi.height, lutY, lutU, lutV, env, cpu_mask_);
    } else {
      coloryuv_apply_lut_planar(dst->GetWritePtr(), src->GetReadPtr(), dst->GetPitch(), src->GetPitch(), vi.width,
                                vi.height, lutY, bits_per_pixel, env, cpu_mask_);
      if (!vi.IsY()) {
        const int width = vi.width >> vi.GetPlaneWidthSubsampling(PLANAR_U);
        const int height = vi.height >> vi.GetPlaneHeightSubsampling(PLANAR_U);

        coloryuv_apply_lut_planar(dst->GetWritePtr(PLANAR_U), src->GetReadPtr(PLANAR_U), dst->GetPitch(PLANAR_U),
                                  src->GetPitch(PLANAR_U), width, height, lutU, bits_per_pixel, env, cpu_mask_);
        coloryuv_apply_lut_planar(dst->GetWritePtr(PLANAR_V), src->GetReadPtr(PLANAR_V), dst->GetPitch(PLANAR_V),
                                  src->GetPitch(PLANAR_V), width, height, lutV, bits_per_pixel, env, cpu_mask_);
      }
      if (vi.IsYUVA()) {
        env->BitBlt(dst->GetWritePtr(PLANAR_A), dst->GetPitch(PLANAR_A), src->GetReadPtr(PLANAR_A),
                    src->GetPitch(PLANAR_A), src->GetRowSize(PLANAR_A), src->GetHeight(PLANAR_A));
      }
    }

    if (luts_live[0])
      delete[] luts_live[0];
    if (luts_live[1])
      delete[] luts_live[1];
    if (luts_live[2])
      delete[] luts_live[2];
  } // lut create and use
  else {
    // 32 bit float: expr
    std::string exprY = coloryuv_create_lut_expr(&cY, bits_per_pixel, tweaklike_params);
    std::string exprU = !vi.IsY() ? coloryuv_create_lut_expr(&cU, bits_per_pixel, tweaklike_params) : "";
    std::string exprV = !vi.IsY() ? coloryuv_create_lut_expr(&cV, bits_per_pixel, tweaklike_params) : "";
    std::string exprA = ""; // copy
    // Invoke Expr
    AVSValue child2;
    if (vi.IsY()) {
      AVSValue new_args[2] = {child, exprY.c_str()};
      child2 = env->Invoke("Expr", AVSValue(new_args, 2)).AsClip();
    } else if (vi.IsYUV()) {
      AVSValue new_args[4] = {child, exprY.c_str(), exprU.c_str(), exprV.c_str()};
      child2 = env->Invoke("Expr", AVSValue(new_args, 4)).AsClip();
    } else if (vi.IsYUVA()) {
      AVSValue new_args[5] = {child, exprY.c_str(), exprU.c_str(), exprV.c_str(), exprA.c_str()};
      child2 = env->Invoke("Expr", AVSValue(new_args, 5)).AsClip();
    }
    dst = child2.AsClip()->GetFrame(n, env);
  }

  if (analyse) {
    env->ApplyMessage(&dst, vi, text, vi.width / 4, 0xa0a0a0, 0, 0);
  }

  // when there was no such property from constructor and it could not be guessed then we do not put one
  if (theColorRange == ColorRange_e::AVS_RANGE_FULL || theColorRange == ColorRange_e::AVS_RANGE_LIMITED) {
    auto props = env->getFramePropsRW(dst);
    update_color_yuv_range(props, theColorRange, env);
  }

  return dst;
}

AVSValue __cdecl ColorYUV::Create(AVSValue args, void*, IScriptEnvironment* env) {
  const bool tweaklike_params = args[23].AsBool(false); // f2c = true: for tweak-like parameter interpretation
  const float def = tweaklike_params ? 1.0f : 0.0f;
  return new ColorYUV(args[0].AsClip(),
                      args[1].AsFloat(def),   // gain_y
                      args[2].AsFloat(0.0f),  // off_y      bright
                      args[3].AsFloat(def),   // gamma_y
                      args[4].AsFloat(def),   // cont_y
                      args[5].AsFloat(def),   // gain_u
                      args[6].AsFloat(0.0f),  // off_u      bright
                      args[7].AsFloat(def),   // gamma_u
                      args[8].AsFloat(def),   // cont_u
                      args[9].AsFloat(def),   // gain_v
                      args[10].AsFloat(0.0f), // off_v
                      args[11].AsFloat(def),  // gamma_v
                      args[12].AsFloat(def),  // cont_v
                      args[13].AsString(""),  // levels = "", "TV->PC", "PC->TV"
                      args[14].AsString(""),  // opt = "", "coring"
                      //                      args[15].AsString(""),                // matrix = "", "rec.709"
                      args[16].AsBool(false), // colorbars
                      args[17].AsBool(false), // analyze
                      args[18].AsBool(false), // autowhite
                      args[19].AsBool(false), // autogain
                      args[20].AsBool(false), // conditional
                      args[21].AsInt(8),      // bits avs+
                      args[22].AsBool(false), // showyuv_fullrange avs+
                      tweaklike_params, // for gain, gamma, cont: 0.0/0.5/1.0/2.0/3.0 instead of -256/-128/0/256/512
                      args[24].AsString(""),  // condvarsuffix avs+
                      args[25].AsBool(false), // optForceUseExpr debug parameter
                      env);
}
