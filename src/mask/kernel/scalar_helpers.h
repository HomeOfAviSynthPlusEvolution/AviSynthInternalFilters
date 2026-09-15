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
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>
using BYTE = uint8_t;
constexpr int cyb = 3736, cyg = 19234, cyr = 9798;
constexpr float cyb_f = 0.114f, cyg_f = 0.587f, cyr_f = 0.299f;
inline bool IsClose(int a, int b, unsigned t) {
  return std::abs(a - b) <= int(t);
}
inline bool IsCloseFloat(float a, float b, float t) {
  return std::abs(a - b) <= t;
}
template <class T>
void fill_plane(uint8_t* p, int height, int row, int pitch, T value) {
  for (int y = 0; y < height; ++y, p += pitch)
    std::fill_n(reinterpret_cast<T*>(p), row / int(sizeof(T)), value);
}
template <typename pixel_t>
inline void mask_c(BYTE* srcp8, const BYTE* alphap8, int src_pitch, int alpha_pitch, size_t width, size_t height) {
  pixel_t* srcp = reinterpret_cast<pixel_t*>(srcp8);
  const pixel_t* alphap = reinterpret_cast<const pixel_t*>(alphap8);

  src_pitch /= sizeof(pixel_t);
  alpha_pitch /= sizeof(pixel_t);

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      srcp[x * 4 + 3] = (cyb * alphap[x * 4 + 0] + cyg * alphap[x * 4 + 1] + cyr * alphap[x * 4 + 2] + 16384) >> 15;
    }
    srcp += src_pitch;
    alphap += alpha_pitch;
  }
}

template <typename pixel_t>
inline void mask_planar_rgb_c(BYTE* dstp8, const BYTE* srcp_r8, const BYTE* srcp_g8, const BYTE* srcp_b8, int dst_pitch,
                              int src_pitch, size_t width, size_t height, int /*bits_per_pixel*/) {
  pixel_t* dstp = reinterpret_cast<pixel_t*>(dstp8);
  const pixel_t* srcp_r = reinterpret_cast<const pixel_t*>(srcp_r8);
  const pixel_t* srcp_g = reinterpret_cast<const pixel_t*>(srcp_g8);
  const pixel_t* srcp_b = reinterpret_cast<const pixel_t*>(srcp_b8);
  src_pitch /= sizeof(pixel_t);
  dst_pitch /= sizeof(pixel_t);

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      dstp[x] = ((cyb * srcp_b[x] + cyg * srcp_g[x] + cyr * srcp_r[x] + 16384) >> 15);
    }
    dstp += dst_pitch;
    srcp_r += src_pitch;
    srcp_g += src_pitch;
    srcp_b += src_pitch;
  }
}

inline void mask_planar_rgb_float_c(BYTE* dstp8, const BYTE* srcp_r8, const BYTE* srcp_g8, const BYTE* srcp_b8,
                                    int dst_pitch, int src_pitch, size_t width, size_t height) {

  float* dstp = reinterpret_cast<float*>(dstp8);
  const float* srcp_r = reinterpret_cast<const float*>(srcp_r8);
  const float* srcp_g = reinterpret_cast<const float*>(srcp_g8);
  const float* srcp_b = reinterpret_cast<const float*>(srcp_b8);
  src_pitch /= sizeof(float);
  dst_pitch /= sizeof(float);

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      dstp[x] = cyb_f * srcp_b[x] + cyg_f * srcp_g[x] + cyr_f * srcp_r[x];
    }
    dstp += dst_pitch;
    srcp_r += src_pitch;
    srcp_g += src_pitch;
    srcp_b += src_pitch;
  }
}

template <typename pixel_t>
inline void colorkeymask_c(BYTE* pf8, int pitch, int R, int G, int B, int height, int rowsize, int tolB, int tolG,
                           int tolR) {
  pixel_t* pf = reinterpret_cast<pixel_t*>(pf8);
  rowsize /= sizeof(pixel_t);
  pitch /= sizeof(pixel_t);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < rowsize; x += 4) {
      if (IsClose(pf[x], B, tolB) && IsClose(pf[x + 1], G, tolG) && IsClose(pf[x + 2], R, tolR))
        pf[x + 3] = 0;
    }
    pf += pitch;
  }
}

template <typename pixel_t>
inline void colorkeymask_planar_c(const BYTE* pfR8, const BYTE* pfG8, const BYTE* pfB8, BYTE* pfA8, int pitch, int R,
                                  int G, int B, int height, int width, int tolB, int tolG, int tolR) {
  const pixel_t* pfR = reinterpret_cast<const pixel_t*>(pfR8);
  const pixel_t* pfG = reinterpret_cast<const pixel_t*>(pfG8);
  const pixel_t* pfB = reinterpret_cast<const pixel_t*>(pfB8);
  pixel_t* pfA = reinterpret_cast<pixel_t*>(pfA8);
  pitch /= sizeof(pixel_t);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      if (IsClose(pfB[x], B, tolB) && IsClose(pfG[x], G, tolG) && IsClose(pfR[x], R, tolR))
        pfA[x] = 0;
    }
    pfR += pitch;
    pfG += pitch;
    pfB += pitch;
    pfA += pitch;
  }
}

inline void colorkeymask_planar_float_c(const BYTE* pfR8, const BYTE* pfG8, const BYTE* pfB8, BYTE* pfA8, int pitch,
                                        float R, float G, float B, int height, int width, float tolB, float tolG,
                                        float tolR) {
  typedef float pixel_t;
  const pixel_t* pfR = reinterpret_cast<const pixel_t*>(pfR8);
  const pixel_t* pfG = reinterpret_cast<const pixel_t*>(pfG8);
  const pixel_t* pfB = reinterpret_cast<const pixel_t*>(pfB8);
  pixel_t* pfA = reinterpret_cast<pixel_t*>(pfA8);
  pitch /= sizeof(pixel_t);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      if (IsCloseFloat(pfB[x], B, tolB) && IsCloseFloat(pfG[x], G, tolG) && IsCloseFloat(pfR[x], R, tolR))
        pfA[x] = 0;
    }
    pfR += pitch;
    pfG += pitch;
    pfB += pitch;
    pfA += pitch;
  }
}
