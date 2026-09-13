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

#include "common.h"

namespace aif::focus::scalar {
template <typename pixel_t>
static void af_vertical_c(BYTE* line_buf8, BYTE* dstp8, const int height, const int pitch8, const int width,
                          const int half_amount, int bits_per_pixel) {
  typedef typename std::conditional<sizeof(pixel_t) == 1, int, int64_t>::type weight_t;
  // kernel:[(1-1/2^_amount)/2, 1/2^_amount, (1-1/2^_amount)/2]
  weight_t center_weight =
      half_amount * 2; // *2: 16 bit scaled arithmetic, but the converted amount parameter scaled is only 15 bits
  weight_t outer_weight = 32768 - half_amount; // (1-1/2^_amount)/2  32768 = 0.5
  int max_pixel_value = (1 << bits_per_pixel) - 1;

  pixel_t* dstp = reinterpret_cast<pixel_t*>(dstp8);
  pixel_t* line_buf = reinterpret_cast<pixel_t*>(line_buf8);
  int pitch = pitch8 / sizeof(pixel_t);

  for (int y = height - 1; y > 0; --y) {
    for (int x = 0; x < width; ++x) {
      pixel_t a;
      // Note: ScaledPixelClip is overloaded. With int64_t parameter and uint16_t result works for 16 bit
      if constexpr (sizeof(pixel_t) == 1)
        a = ScaledPixelClip((weight_t)(dstp[x] * center_weight + (line_buf[x] + dstp[x + pitch]) * outer_weight));
      else
        a = (pixel_t)ScaledPixelClipEx(
            (weight_t)(dstp[x] * center_weight + (line_buf[x] + dstp[x + pitch]) * outer_weight), max_pixel_value);
      line_buf[x] = dstp[x];
      dstp[x] = a;
    }
    dstp += pitch;
  }
  for (int x = 0; x < width; ++x) { // Last row - map centre as lower
    if constexpr (sizeof(pixel_t) == 1)
      dstp[x] = ScaledPixelClip((weight_t)(dstp[x] * center_weight + (line_buf[x] + dstp[x]) * outer_weight));
    else
      dstp[x] = (pixel_t)ScaledPixelClipEx((weight_t)(dstp[x] * center_weight + (line_buf[x] + dstp[x]) * outer_weight),
                                           max_pixel_value);
  }
}

static void af_vertical_c_float(BYTE* line_buf8, BYTE* dstp8, const int height, const int pitch8, const int width,
                                const float amount) {
  float* dstp = reinterpret_cast<float*>(dstp8);
  float* line_buf = reinterpret_cast<float*>(line_buf8);
  int pitch = pitch8 / sizeof(float);

  const float center_weight = amount;
  const float outer_weight = (1.0f - amount) / 2.0f;

  for (int y = height - 1; y > 0; --y) {
    for (int x = 0; x < width; ++x) {
      float a = dstp[x] * center_weight + (line_buf[x] + dstp[x + pitch]) * outer_weight;
      line_buf[x] = dstp[x];
      dstp[x] = a;
    }
    dstp += pitch;
  }
  for (int x = 0; x < width; ++x) { // Last row - map centre as lower
    dstp[x] = dstp[x] * center_weight + (line_buf[x] + dstp[x]) * outer_weight;
  }
}

template <typename pixel_t>
static void af_vertical_process(BYTE* line_buf, BYTE* dstp, size_t height, size_t pitch, size_t row_size,
                                int half_amount, int bits_per_pixel) {
  size_t width = row_size / sizeof(pixel_t);
  // only for 8/16 bit, float separated
  {
    af_vertical_c<pixel_t>(line_buf, dstp, (int)height, (int)pitch, (int)width, half_amount, bits_per_pixel);
  }
}

static void af_vertical_process_float(BYTE* line_buf, BYTE* dstp, size_t height, size_t pitch, size_t row_size,
                                      double amountd) {
  size_t width = row_size / sizeof(float);
  {
    af_vertical_c_float(line_buf, dstp, (int)height, (int)pitch, (int)width, (float)amountd);
  }
}

template <typename pixel_t, typename weight_t>
static AVS_FORCEINLINE void af_horizontal_rgb32_process_line_c(pixel_t b_left, pixel_t g_left, pixel_t r_left,
                                                               pixel_t a_left, pixel_t* dstp, size_t width,
                                                               weight_t center_weight, weight_t outer_weight) {
  size_t x;
  for (x = 0; x < width - 1; ++x) {
    pixel_t b =
        ScaledPixelClip((weight_t)(dstp[x * 4 + 0] * center_weight + (b_left + dstp[x * 4 + 4]) * outer_weight));
    b_left = dstp[x * 4 + 0];
    dstp[x * 4 + 0] = b;
    pixel_t g =
        ScaledPixelClip((weight_t)(dstp[x * 4 + 1] * center_weight + (g_left + dstp[x * 4 + 5]) * outer_weight));
    g_left = dstp[x * 4 + 1];
    dstp[x * 4 + 1] = g;
    pixel_t r =
        ScaledPixelClip((weight_t)(dstp[x * 4 + 2] * center_weight + (r_left + dstp[x * 4 + 6]) * outer_weight));
    r_left = dstp[x * 4 + 2];
    dstp[x * 4 + 2] = r;
    pixel_t a =
        ScaledPixelClip((weight_t)(dstp[x * 4 + 3] * center_weight + (a_left + dstp[x * 4 + 7]) * outer_weight));
    a_left = dstp[x * 4 + 3];
    dstp[x * 4 + 3] = a;
  }
  dstp[x * 4 + 0] =
      ScaledPixelClip((weight_t)(dstp[x * 4 + 0] * center_weight + (b_left + dstp[x * 4 + 0]) * outer_weight));
  dstp[x * 4 + 1] =
      ScaledPixelClip((weight_t)(dstp[x * 4 + 1] * center_weight + (g_left + dstp[x * 4 + 1]) * outer_weight));
  dstp[x * 4 + 2] =
      ScaledPixelClip((weight_t)(dstp[x * 4 + 2] * center_weight + (r_left + dstp[x * 4 + 2]) * outer_weight));
  dstp[x * 4 + 3] =
      ScaledPixelClip((weight_t)(dstp[x * 4 + 3] * center_weight + (a_left + dstp[x * 4 + 3]) * outer_weight));
}

template <typename pixel_t>
static void af_horizontal_rgb32_64_c(BYTE* dstp8, size_t height, size_t pitch8, size_t width, int half_amount) {
  typedef typename std::conditional<sizeof(pixel_t) == 1, int, int64_t>::type weight_t;
  // kernel:[(1-1/2^_amount)/2, 1/2^_amount, (1-1/2^_amount)/2]
  weight_t center_weight =
      half_amount * 2; // *2: 16 bit scaled arithmetic, but the converted amount parameter scaled is only 15 bits
  weight_t outer_weight = 32768 - half_amount; // (1-1/2^_amount)/2  32768 = 0.5

  pixel_t* dstp = reinterpret_cast<pixel_t*>(dstp8);
  size_t pitch = pitch8 / sizeof(pixel_t);

  for (size_t y = height; y > 0; --y) {
    pixel_t b_left = dstp[0];
    pixel_t g_left = dstp[1];
    pixel_t r_left = dstp[2];
    pixel_t a_left = dstp[3];
    af_horizontal_rgb32_process_line_c<pixel_t, weight_t>(b_left, g_left, r_left, a_left, dstp, width, center_weight,
                                                          outer_weight);
    dstp += pitch;
  }
}

// -------------------------------------
// Blur/Sharpen Horizontal YUY2 C++ Code
// -------------------------------------

static void af_horizontal_yuy2_c(BYTE* p, int height, int pitch, int width, int amount) {
  const int center_weight = amount * 2;
  const int outer_weight = 32768 - amount;
  for (int y0 = height; y0 > 0; --y0) {
    BYTE yy = p[0];
    BYTE uv = p[1];
    BYTE vu = p[3];
    int x;
    for (x = 0; x < width - 2; ++x) {
      BYTE y = ScaledPixelClip(p[x * 2 + 0] * center_weight + (yy + p[x * 2 + 2]) * outer_weight);
      yy = p[x * 2 + 0];
      p[x * 2 + 0] = y;
      BYTE w = ScaledPixelClip(p[x * 2 + 1] * center_weight + (uv + p[x * 2 + 5]) * outer_weight);
      uv = vu;
      vu = p[x * 2 + 1];
      p[x * 2 + 1] = w;
    }
    BYTE y = ScaledPixelClip(p[x * 2 + 0] * center_weight + (yy + p[x * 2 + 2]) * outer_weight);
    yy = p[x * 2 + 0];
    p[x * 2 + 0] = y;
    p[x * 2 + 1] = ScaledPixelClip(p[x * 2 + 1] * center_weight + (uv + p[x * 2 + 1]) * outer_weight);
    p[x * 2 + 2] = ScaledPixelClip(p[x * 2 + 2] * center_weight + (yy + p[x * 2 + 2]) * outer_weight);
    p[x * 2 + 3] = ScaledPixelClip(p[x * 2 + 3] * center_weight + (vu + p[x * 2 + 3]) * outer_weight);

    p += pitch;
  }
}

// --------------------------------------
// Blur/Sharpen Horizontal RGB24 C++ Code
// --------------------------------------

template <typename pixel_t>
static void af_horizontal_rgb24_48_c(BYTE* dstp8, int height, int pitch8, int width, int half_amount) {
  typedef typename std::conditional<sizeof(pixel_t) == 1, int, int64_t>::type weight_t;
  // kernel:[(1-1/2^_amount)/2, 1/2^_amount, (1-1/2^_amount)/2]
  weight_t center_weight =
      half_amount * 2; // *2: 16 bit scaled arithmetic, but the converted amount parameter scaled is only 15 bits
  weight_t outer_weight = 32768 - half_amount; // (1-1/2^_amount)/2  32768 = 0.5

  pixel_t* dstp = reinterpret_cast<pixel_t*>(dstp8);
  int pitch = pitch8 / sizeof(pixel_t);
  for (int y = height; y > 0; --y) {
    pixel_t bb = dstp[0];
    pixel_t gg = dstp[1];
    pixel_t rr = dstp[2];
    int x;
    for (x = 0; x < width - 1; ++x) {
      // ScaledPixelClip has 2 overloads: BYTE/uint16_t (int/int64 i)
      pixel_t b = ScaledPixelClip((weight_t)(dstp[x * 3 + 0] * center_weight + (bb + dstp[x * 3 + 3]) * outer_weight));
      bb = dstp[x * 3 + 0];
      dstp[x * 3 + 0] = b;
      pixel_t g = ScaledPixelClip((weight_t)(dstp[x * 3 + 1] * center_weight + (gg + dstp[x * 3 + 4]) * outer_weight));
      gg = dstp[x * 3 + 1];
      dstp[x * 3 + 1] = g;
      pixel_t r = ScaledPixelClip((weight_t)(dstp[x * 3 + 2] * center_weight + (rr + dstp[x * 3 + 5]) * outer_weight));
      rr = dstp[x * 3 + 2];
      dstp[x * 3 + 2] = r;
    }
    dstp[x * 3 + 0] =
        ScaledPixelClip((weight_t)(dstp[x * 3 + 0] * center_weight + (bb + dstp[x * 3 + 0]) * outer_weight));
    dstp[x * 3 + 1] =
        ScaledPixelClip((weight_t)(dstp[x * 3 + 1] * center_weight + (gg + dstp[x * 3 + 1]) * outer_weight));
    dstp[x * 3 + 2] =
        ScaledPixelClip((weight_t)(dstp[x * 3 + 2] * center_weight + (rr + dstp[x * 3 + 2]) * outer_weight));
    dstp += pitch;
  }
}

// -------------------------------------
// Blur/Sharpen Horizontal YV12 C++ Code
// -------------------------------------

// for linker reasons these forceinlined functions appear in C, intel sse2 and avx2 source as well
template <typename pixel_t>
static AVS_FORCEINLINE void af_horizontal_planar_process_line_c(pixel_t left, BYTE* dstp8, size_t row_size,
                                                                int center_weight, int outer_weight) {
  size_t x;
  pixel_t* dstp = reinterpret_cast<pixel_t*>(dstp8);
  typedef typename std::conditional<sizeof(pixel_t) == 1, int, int64_t>::type
      weight_t; // for calling the right ScaledPixelClip()
  size_t width = row_size / sizeof(pixel_t);
  for (x = 0; x < width - 1; ++x) {
    pixel_t temp =
        ScaledPixelClip((weight_t)(dstp[x] * (weight_t)center_weight + (left + dstp[x + 1]) * (weight_t)outer_weight));
    left = dstp[x];
    dstp[x] = temp;
  }
  // ScaledPixelClip has 2 overloads: BYTE/uint16_t (int/int64 i)
  dstp[x] = ScaledPixelClip((weight_t)(dstp[x] * (weight_t)center_weight + (left + dstp[x]) * (weight_t)outer_weight));
}

static AVS_FORCEINLINE void af_horizontal_planar_process_line_uint16_c(uint16_t left, BYTE* dstp8, size_t row_size,
                                                                       int center_weight, int outer_weight,
                                                                       int bits_per_pixel) {
  size_t x;
  typedef uint16_t pixel_t;
  pixel_t* dstp = reinterpret_cast<pixel_t*>(dstp8);
  const int max_pixel_value = (1 << bits_per_pixel) - 1; // clamping on 10-12-14-16 bitdepth
  typedef std::conditional<sizeof(pixel_t) == 1, int, int64_t>::type
      weight_t; // for calling the right ScaledPixelClip()
  size_t width = row_size / sizeof(pixel_t);
  for (x = 0; x < width - 1; ++x) {
    pixel_t temp = (pixel_t)ScaledPixelClipEx(
        (weight_t)(dstp[x] * (weight_t)center_weight + (left + dstp[x + 1]) * (weight_t)outer_weight), max_pixel_value);
    left = dstp[x];
    dstp[x] = temp;
  }
  // ScaledPixelClip has 2 overloads: BYTE/uint16_t (int/int64 i)
  dstp[x] = ScaledPixelClipEx((weight_t)(dstp[x] * (weight_t)center_weight + (left + dstp[x]) * (weight_t)outer_weight),
                              max_pixel_value);
}

static AVS_FORCEINLINE void af_horizontal_planar_process_line_float_c(float left, float* dstp, size_t row_size,
                                                                      float center_weight, float outer_weight) {
  size_t x;
  size_t width = row_size / sizeof(float);
  for (x = 0; x < width - 1; ++x) {
    float temp = dstp[x] * center_weight + (left + dstp[x + 1]) * outer_weight;
    left = dstp[x];
    dstp[x] = temp;
  }
  dstp[x] = dstp[x] * center_weight + (left + dstp[x]) * outer_weight;
}

template <typename pixel_t>
static void af_horizontal_planar_c(BYTE* dstp8, size_t height, size_t pitch8, size_t row_size, size_t half_amount,
                                   int bits_per_pixel) {
  pixel_t* dstp = reinterpret_cast<pixel_t*>(dstp8);
  size_t pitch = pitch8 / sizeof(pixel_t);
  int center_weight = int(half_amount * 2);
  int outer_weight = int(32768 - half_amount);
  pixel_t left;
  for (size_t y = height; y > 0; --y) {
    left = dstp[0];
    if constexpr (sizeof(pixel_t) == 1)
      af_horizontal_planar_process_line_c<pixel_t>(left, (BYTE*)dstp, row_size, center_weight, outer_weight);
    else
      af_horizontal_planar_process_line_uint16_c(left, (BYTE*)dstp, row_size, center_weight, outer_weight,
                                                 bits_per_pixel);
    dstp += pitch;
  }
}

static void af_horizontal_planar_float_c(BYTE* dstp8, size_t height, size_t pitch8, size_t row_size, float amount) {
  float* dstp = reinterpret_cast<float*>(dstp8);
  size_t pitch = pitch8 / sizeof(float);
  float center_weight = amount;
  float outer_weight = (1.0f - amount) / 2.0f;
  float left;
  for (size_t y = height; y > 0; --y) {
    left = dstp[0];
    af_horizontal_planar_process_line_float_c(left, dstp, row_size, center_weight, outer_weight);
    dstp += pitch;
  }
}

// offset is the initial value of x. Used when C routine processes only parts of frames after SSE/MMX paths do
// their job.
template <typename pixel_t, bool maxThreshold>
static void accumulate_line_c(BYTE* _c_plane, const BYTE** planeP, int planes, int offset, size_t rowsize,
                              BYTE _threshold, int div, int bits_per_pixel) {
  pixel_t* c_plane = reinterpret_cast<pixel_t*>(_c_plane);

  typedef typename std::conditional<sizeof(pixel_t) <= 2, int, float>::type sum_t;
  typedef typename std::conditional<sizeof(pixel_t) == 1, int, int64_t>::type bigsum_t;
  typedef typename std::conditional<std::is_floating_point<pixel_t>::value, float, int>::type threshold_t;

  size_t width = rowsize / sizeof(pixel_t);

  threshold_t threshold = _threshold; // parameter 0..255
  if (std::is_floating_point<pixel_t>::value)
    threshold = (threshold_t)(threshold / 255.0f); // float
  else if constexpr (sizeof(pixel_t) == 2) {
    threshold = threshold * (uint16_t)(1 << (bits_per_pixel - 8)); // uint16_t, 10 bit: *4 16bit: *256
  }

  float average_multiplier = 0.0f;
  if constexpr (sizeof(pixel_t) == 2) {
    // Match the 16-bit SSE paths' float reciprocal and default nearest rounding.
    average_multiplier = 1.0f / static_cast<float>(planes + 1);
  }

  for (size_t x = offset; x < width; ++x) {
    pixel_t current = c_plane[x];
    sum_t sum = current;

    for (int plane = planes - 1; plane >= 0; plane--) {
      pixel_t p = reinterpret_cast<const pixel_t*>(planeP[plane])[x];
      if (maxThreshold) {
        sum += p; // simple frame average mode
      } else {
        sum_t absdiff = std::abs(current - p);

        if (absdiff <= threshold) {
          sum += p;
        } else {
          sum += current;
        }
      }
    }
    if (std::is_floating_point<pixel_t>::value)
      c_plane[x] = (pixel_t)(sum / (planes + 1)); // float: simple average
    else if constexpr (sizeof(pixel_t) == 2)
      c_plane[x] = (pixel_t)std::nearbyintf(static_cast<float>(sum) * average_multiplier);
    else
      c_plane[x] = (pixel_t)(((bigsum_t)sum * div + 16384) >> 15); // div = 32768/(planes+1) for integer arithmetic
  }
}

static void accumulate_line_yuy2_c(BYTE* c_plane, const BYTE** planeP, int planes, size_t width, BYTE threshold_luma,
                                   BYTE threshold_chroma, int div) {
  for (size_t x = 0; x < width; x += 2) {
    BYTE current_y = c_plane[x];
    BYTE current_c = c_plane[x + 1];
    size_t sum_y = current_y;
    size_t sum_c = current_c;

    for (int plane = planes - 1; plane >= 0; plane--) {
      BYTE p_y = planeP[plane][x];
      BYTE p_c = planeP[plane][x + 1];
      size_t absdiff_y = std::abs(current_y - p_y);
      size_t absdiff_c = std::abs(current_c - p_c);

      if (absdiff_y <= threshold_luma) {
        sum_y += p_y;
      } else {
        sum_y += current_y;
      }

      if (absdiff_c <= threshold_chroma) {
        sum_c += p_c;
      } else {
        sum_c += current_c;
      }
    }

    c_plane[x] = (BYTE)((sum_y * div + 16384) >> 15);
    c_plane[x + 1] = (BYTE)((sum_c * div + 16384) >> 15);
  }
}

static void accumulate_line_yuy2(BYTE* c_plane, const BYTE** planeP, int planes, size_t width, BYTE threshold_luma,
                                 BYTE threshold_chroma, int div) {
  accumulate_line_yuy2_c(c_plane, planeP, planes, width, threshold_luma, threshold_chroma, div);
}

static void accumulate_line(BYTE* c_plane, const BYTE** planeP, int planes, size_t rowsize, BYTE threshold, int div,
                            int pixelsize, int bits_per_pixel) {
  // threshold == 255: simple average
  bool maxThreshold = (threshold == 255);

  {
    switch (pixelsize) {
      case 1:
        if (maxThreshold)
          accumulate_line_c<uint8_t, true>(c_plane, planeP, planes, 0, rowsize, threshold, div, bits_per_pixel);
        else
          accumulate_line_c<uint8_t, false>(c_plane, planeP, planes, 0, rowsize, threshold, div, bits_per_pixel);
        break;
      case 2:
        if (maxThreshold)
          accumulate_line_c<uint16_t, true>(c_plane, planeP, planes, 0, rowsize, threshold, div, bits_per_pixel);
        else
          accumulate_line_c<uint16_t, false>(c_plane, planeP, planes, 0, rowsize, threshold, div, bits_per_pixel);
        break;
      case 4:
        if (maxThreshold)
          accumulate_line_c<float, true>(c_plane, planeP, planes, 0, rowsize, threshold, div, bits_per_pixel);
        else
          accumulate_line_c<float, false>(c_plane, planeP, planes, 0, rowsize, threshold, div, bits_per_pixel);
        break;
    }
  }
}

template <typename pixel_t>
static int64_t calculate_sad_c(const BYTE* cur_ptr, const BYTE* other_ptr, int cur_pitch, int other_pitch,
                               size_t rowsize, size_t height) {
  const pixel_t* ptr1 = reinterpret_cast<const pixel_t*>(cur_ptr);
  const pixel_t* ptr2 = reinterpret_cast<const pixel_t*>(other_ptr);
  size_t width = rowsize / sizeof(pixel_t);
  cur_pitch /= sizeof(pixel_t);
  other_pitch /= sizeof(pixel_t);

  // for fullframe float may loose precision
  typedef typename std::conditional<std::is_floating_point<pixel_t>::value, double, int64_t>::type sum_t;
  // for one row int is enough and faster than int64
  typedef typename std::conditional<std::is_floating_point<pixel_t>::value, float, int>::type sumrow_t;
  sum_t sum = 0;

  for (size_t y = 0; y < height; ++y) {
    sumrow_t sumrow = 0;
    for (size_t x = 0; x < width; ++x) {
      sumrow += std::abs(ptr1[x] - ptr2[x]);
    }
    sum += sumrow;
    ptr1 += cur_pitch;
    ptr2 += other_pitch;
  }
  if (std::is_floating_point<pixel_t>::value)
    return (int64_t)(sum * 255); // scale 0..1 based sum to 8 bit range
  else
    return (int64_t)sum; // for int, scaling to 8 bit range is done outside
}

// sum of byte-diffs.
static int64_t calculate_sad(const BYTE* cur_ptr, const BYTE* other_ptr, int cur_pitch, int other_pitch, size_t rowsize,
                             size_t height, int pixelsize, int bits_per_pixel) {

  switch (pixelsize) {
    case 1:
      return calculate_sad_c<uint8_t>(cur_ptr, other_ptr, cur_pitch, other_pitch, rowsize, height);
    case 2:
      return calculate_sad_c<uint16_t>(cur_ptr, other_ptr, cur_pitch, other_pitch, rowsize, height) >>
             (bits_per_pixel - 8); // scale back to 8 bit range;
    default:                       // case 4
      return calculate_sad_c<float>(cur_ptr, other_ptr, cur_pitch, other_pitch, rowsize, height);
  }
}

} // namespace aif::focus::scalar
