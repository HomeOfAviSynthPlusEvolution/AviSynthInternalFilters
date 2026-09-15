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

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
template <typename pixel_t>
static void fill_accum_rgb_planar_c(const BYTE* srcpR, const BYTE* srcpG, const BYTE* srcpB, int pitch,
                                    unsigned int* accum_r, unsigned int* accum_g, unsigned int* accum_b, int width,
                                    int height, int max_pixel_value) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int r = reinterpret_cast<const pixel_t*>(srcpR)[x];
      int g = reinterpret_cast<const pixel_t*>(srcpG)[x];
      int b = reinterpret_cast<const pixel_t*>(srcpB)[x];
      if constexpr (sizeof(pixel_t) != 1) {
        if (r > max_pixel_value)
          r = max_pixel_value;
        if (g > max_pixel_value)
          g = max_pixel_value;
        if (b > max_pixel_value)
          b = max_pixel_value;
      }
      accum_r[r]++;
      accum_g[g]++;
      accum_b[b]++;
    }
    srcpR += pitch;
    srcpG += pitch;
    srcpB += pitch;
  }
}

static void fill_accum_rgb_planar_float_c(const BYTE* srcpR, const BYTE* srcpG, const BYTE* srcpB, int pitch,
                                          unsigned int* accum_r, unsigned int* accum_g, unsigned int* accum_b,
                                          int width, int height, RGBStats& rgbplanedata) {

  for (int i = 0; i < 3; i++) {
    rgbplanedata.data[i].real_max = std::numeric_limits<float>::min();
    rgbplanedata.data[i].real_min = std::numeric_limits<float>::max();
    rgbplanedata.data[i].sum = 0.0;
  }

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      // convert range 0..1 to 0..65535 for loose_min/max fake histogram
      float r = reinterpret_cast<const float*>(srcpR)[x];
      float g = reinterpret_cast<const float*>(srcpG)[x];
      float b = reinterpret_cast<const float*>(srcpB)[x];
      int ri = (int)(clamp(r * 65535.0f + 0.5f, 0.0f, 65535.0f));
      int gi = (int)(clamp(g * 65535.0f + 0.5f, 0.0f, 65535.0f));
      int bi = (int)(clamp(b * 65535.0f + 0.5f, 0.0f, 65535.0f));
      if (r > rgbplanedata.data[0].real_max)
        rgbplanedata.data[0].real_max = r;
      if (r < rgbplanedata.data[0].real_min)
        rgbplanedata.data[0].real_min = r;
      rgbplanedata.data[0].sum += r;
      if (g > rgbplanedata.data[1].real_max)
        rgbplanedata.data[1].real_max = g;
      if (g < rgbplanedata.data[1].real_min)
        rgbplanedata.data[1].real_min = g;
      rgbplanedata.data[1].sum += g;
      if (b > rgbplanedata.data[2].real_max)
        rgbplanedata.data[2].real_max = b;
      if (b < rgbplanedata.data[2].real_min)
        rgbplanedata.data[2].real_min = b;
      rgbplanedata.data[2].sum += b;
      accum_r[ri]++;
      accum_g[gi]++;
      accum_b[bi]++;
    }
    srcpR += pitch;
    srcpG += pitch;
    srcpB += pitch;
  }
}

template <typename pixel_t>
static void fill_accum_rgb_packed_c(const BYTE* srcp, int pitch, unsigned int* accum_r, unsigned int* accum_g,
                                    unsigned int* accum_b, int work_width, int height, int pixel_step) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < work_width; x += pixel_step) {
      accum_r[reinterpret_cast<const pixel_t*>(srcp)[x + 2]]++;
      accum_g[reinterpret_cast<const pixel_t*>(srcp)[x + 1]]++;
      accum_b[reinterpret_cast<const pixel_t*>(srcp)[x + 0]]++;
    }
    srcp += pitch;
  }
}

template <typename pixel_t, int pixel_step, bool dither>
static void apply_map_rgb_packed_c(BYTE* dstp8, int pitch, BYTE* mapR, BYTE* mapG, BYTE* mapB, BYTE* mapA, int width,
                                   int height, int, IScriptEnvironment* env, uint32_t cpu_mask) {
  if constexpr (!dither) {
    const BYTE* maps[4] = {mapB, mapG, mapR, mapA};
    for (int c = 0; c < pixel_step; ++c) {
      auto* p = dstp8 + c * sizeof(pixel_t);
      map_channel(p, pitch, p, pitch, width, height, maps[c], sizeof(pixel_t) == 1 ? 8 : 16, pixel_step, env, cpu_mask);
    }
    return;
  }

  int _y = 0;
  int _dither = 0;
  pixel_t* dstp = reinterpret_cast<pixel_t*>(dstp8);
  pitch /= sizeof(pixel_t);

  for (int y = 0; y < height; y++) {
    if (dither)
      _y = (y << 4) & 0xf0;
    for (int x = 0; x < width; x++) {
      if (dither)
        _dither = ditherMap[(x & 0x0f) | _y];
      dstp[x * pixel_step + 0] =
          reinterpret_cast<pixel_t*>(mapB)[dither ? dstp[x * pixel_step + 0] << 8 | _dither : dstp[x * pixel_step + 0]];
      dstp[x * pixel_step + 1] =
          reinterpret_cast<pixel_t*>(mapG)[dither ? dstp[x * pixel_step + 1] << 8 | _dither : dstp[x * pixel_step + 1]];
      dstp[x * pixel_step + 2] =
          reinterpret_cast<pixel_t*>(mapR)[dither ? dstp[x * pixel_step + 2] << 8 | _dither : dstp[x * pixel_step + 2]];
      if constexpr (pixel_step == 4)
        dstp[x * pixel_step + 3] = reinterpret_cast<pixel_t*>(
            mapA)[dither ? dstp[x * pixel_step + 3] << 8 | _dither : dstp[x * pixel_step + 3]];
    }
    dstp += pitch;
  }
}

template <typename pixel_t, bool hasAlpha, bool dither>
static void apply_map_rgb_planar_c(BYTE* dstpR8, BYTE* dstpG8, BYTE* dstpB8, BYTE* dstpA8, int pitch, BYTE* mapR,
                                   BYTE* mapG, BYTE* mapB, BYTE* mapA, int width, int height, int,
                                   IScriptEnvironment* env, uint32_t cpu_mask) {
  if constexpr (!dither) {
    BYTE* planes[4] = {dstpR8, dstpG8, dstpB8, dstpA8};
    const BYTE* maps[4] = {mapR, mapG, mapB, mapA};
    for (int c = 0; c < (hasAlpha ? 4 : 3); ++c)
      map_channel(planes[c], pitch, planes[c], pitch, width, height, maps[c], sizeof(pixel_t) == 1 ? 8 : 16, 1, env,
                  cpu_mask);
    return;
  }

  int _y = 0;
  int _dither = 0;
  pixel_t* dstpR = reinterpret_cast<pixel_t*>(dstpR8);
  pixel_t* dstpG = reinterpret_cast<pixel_t*>(dstpG8);
  pixel_t* dstpB = reinterpret_cast<pixel_t*>(dstpB8);
  pixel_t* dstpA = reinterpret_cast<pixel_t*>(dstpA8);
  pitch /= sizeof(pixel_t);

  for (int y = 0; y < height; y++) {
    if (dither)
      _y = (y << 4) & 0xf0;
    for (int x = 0; x < width; x++) {
      if (dither)
        _dither = ditherMap[(x & 0x0f) | _y];
      reinterpret_cast<pixel_t*>(dstpG)[x] =
          reinterpret_cast<pixel_t*>(mapG)[dither ? dstpG[x] << 8 | _dither : dstpG[x]];
      reinterpret_cast<pixel_t*>(dstpB)[x] =
          reinterpret_cast<pixel_t*>(mapB)[dither ? dstpB[x] << 8 | _dither : dstpB[x]];
      reinterpret_cast<pixel_t*>(dstpR)[x] =
          reinterpret_cast<pixel_t*>(mapR)[dither ? dstpR[x] << 8 | _dither : dstpR[x]];
      if (hasAlpha)
        reinterpret_cast<pixel_t*>(dstpA)[x] =
            reinterpret_cast<pixel_t*>(mapA)[dither ? dstpA[x] << 8 | _dither : dstpA[x]];
    }
    dstpG += pitch;
    dstpB += pitch;
    dstpR += pitch;
    if (hasAlpha)
      dstpA += pitch;
  }
}
