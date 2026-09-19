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

// Adapted from AviSynth+ master 5c82777b374bdef16e13007a11e77d735ac1e4eb.
#include "backend.h"
#include "color_constants.h"
#include <cstring>
#include <type_traits>
#include <vector>
namespace aif::color_bars {
struct FloatRange {
  float mul_factor;
  float dst_offset;
};
static void GetYUVRec709fromRGB(double R, double G, double B, double& dY, double& dU, double& dV) {
  // See 3.2 from https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.709-6-201506-I!!PDF-E.pdf
  double Kr, Kb;
  Kr = 0.2126;
  Kb = 0.0722;
  dY = Kr * R + (1.0 - Kr - Kb) * G + Kb * B;
  dU = (B - dY) / (2.0 * (1.0 - Kb));
  dV = (R - dY) / (2.0 * (1.0 - Kr));
}

template <typename pixel_t>
static void draw_colorbarsHD_444(uint8_t* pY8, uint8_t* pU8, uint8_t* pV8, int pitchY, int pitchUV, int w, int h,
                                 int bits_per_pixel, Copy copy) {
  pixel_t* pY = reinterpret_cast<pixel_t*>(pY8);
  pixel_t* pU = reinterpret_cast<pixel_t*>(pU8);
  pixel_t* pV = reinterpret_cast<pixel_t*>(pV8);
  pitchY /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

  const int shift = sizeof(pixel_t) == 4 ? 0 : (bits_per_pixel - 8);

  // Also for float target we make "limited" range
  // Match upstream float rounding before promotion to double.
  constexpr FloatRange luma{219.0f / 255.0f, 16.0f / 255.0f};
  constexpr FloatRange chroma{(112.0f / 255.0f) / 0.5f, 0.0f};
  double float_offset = luma.dst_offset;
  double float_scale = luma.mul_factor; // 219.0 / 255.0;
  double float_uv_scale = chroma.mul_factor;

  //		Nearest 16:9 pixel exact sizes
  //		56*X x 12*Y
  //		 728 x  480  ntsc anamorphic
  //		 728 x  576  pal anamorphic
  //		 840 x  480
  //		1008 x  576
  //		1288 x  720 <- default
  //		1456 x 1080  hd anamorphic
  //		1904 x 1080
  /*
  ARIB STD-B28  Version 1.0-E1

  *1: 75W/100W/I+: Choice from 75% white, 100% white and +I signal. Avisynth: I+.
  *2: can be changed to any value other than the standard values in accordance with the operation purpose by the user

              |<-------------------------------------------------------- a -------------------------------------------------->|
              |             |<------------------------------------------3/4 a --------------------------------->|             |
              |<---- d ---->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<---- d ---->|

              +-------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-------------+  -----------
              |             |           |           |           |           |           |           |           |             |      ^   ^
              |             |           |           |           |           |           |           |           |             |      |   |
Pattern 1     |     40%     |    75%    |    75%    |    75%    |    75%    |    75%    |    75%    |    75%    |    40%      |      |   |
              |    Grey     |   White   |   Yellow  |   Cyan    |   Green   |  Magenta  |   Red     |   Blue    |    Grey     | 7/12b|   |
              |     *2      |           |           |           |           |           |           |           |    *2       |      |   |
              |             |           |           |           |           |           |           |           |             |      V   |
              +-------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-------------+ -------  |
Pattern 2     | 100% Cyan   |75W/100W/I+|                          75% white (chroma set signal)                | 100% blue   | 1/12b|   | b
              +-------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-------------+ ------   |
Pattern 3     | 100% Yellow |                                       Y ramp                                      | 100% red    | 1/12b|   |
              +-------------+-----------------+-----------+-----------+-------+----+----+---+---+---+-----------+-------------+ -------  |
              |     *2      |                 |                       |       |    |    |   |   |   |           |      *2     |      ^   |
Pattern 4     |     15%     |        0%       |          100%         |   0%  |-2% | 0  |+2%| 0 |+4%|     0%    |     15%     | 3/12 |   |
              |    Grey     |       Black     |         White         | Black |    |    |   |   |   |   Black   |    Grey     |  b   V   V
              +-------------+-----------------+-----------+-----------+-------+----+----+---+---+---+-----------+-------------+  -----------

              |<---- d ---->|<---- 3/2 c ---->|<--------- 2c -------->|<5/6c->|1/3c|1/3c|1/3|1/3|1/3|<--- c --->|<---- d ---->|

              a:b = 16:9


  2021: SMPTE RP 219-1:2014
  *1: can be changed to any value other than the standard values in accordance with the operation purpose by the user
  *2: 75W/100W/+I/-I: Choice from 75% white, 100% white and +I or -I signal. Avisynth: 100% White.
  *3: Choice from 0% Black or +Q (Left from Y ramp) Avisynth: 0% Black.
  *4: can be changed to any value other than the standard values in accordance with the operation purpose by the user
  *5: Choice from 0% Black, Sub-black valley. Avisynth: 0% Black.
      The sub-black valley signal shall begin at the 0% black level, shall decrease in a linear ramp to the minimum permitted level at the mid-point,
      and shall increase in a linear ramp to the 0% black level at the end of the black bar.
  *6: Choice from 100% White, Super-white Peak. Avisynth: 100% White.
      The super-white peak signal shall begin at the 100% white level, shall increase in a linear ramp to the maximum permitted level at the midpoint,
      and shall decrease in a linear ramp to the 100% white level at the end of the white bar.

                            |<-------------------------------------------------------- a -------------------------------------------------->|
              |             |<------------------------------------------3/4 a --------------------------------->|             |
              |<---- d ---->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<--- c --->|<---- d ---->|

              +-------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-------------+  -----------
              |             |           |           |           |           |           |           |           |             |      ^   ^
              |             |           |           |           |           |           |           |           |             |      |   |
Pattern 1     |     40%     |    75%    |    75%    |    75%    |    75%    |    75%    |    75%    |    75%    |    40%      |      |   |
              |    Grey     |   White   |   Yellow  |   Cyan    |   Green   |  Magenta  |   Red     |   Blue    |    Grey     | 7/12b|   |
              |     *1      |           |           |           |           |           |           |           |    *1       |      |   |
              |             |           |           |           |           |           |           |           |             |      V   |
              +-------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-------------+ -------  |
Pattern 2     | 100% Cyan   |75/100W/I-+|                          75% white (chroma set signal)                | 100% blue   | 1/12b|   | b
              +-------------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-------------+ ------   |
Pattern 3     | 100% Yellow |0%Blk or +Q|                           Y ramp                          | 100% White| 100% red    | 1/12b|   |
              +-------------+-----------+-----+-----------+-----------+-------+----+----+---+---+---+-----------+-------------+ -------  |
              |     *4      |   0% Black    *5|      100% White     *6|       |    |    |   |   |   |           |      *4     |      ^   |
Pattern 4     |     15%     |0% Blk or SubBlck|100%White/SuperWhtePeak|   0%  |-2% | 0  |+2%| 0 |+4%|     0%    |     15%     | 3/12 |   |
              |    Grey     |   0%  Black     |      100% White       | Black |    |    |   |   |   |   Black   |    Grey     |  b   V   V
              +-------------+-----------------+-----------+-----------+-------+----+----+---+---+---+-----------+-------------+  -----------

              |<---- d ---->|<---- 3/2 c ---->|<--------- 2c -------->|<5/6c->|1/3c|1/3c|1/3|1/3|1/3|<--- c --->|<---- d ---->|

              a:b = 16:9

*/

  int y = 0;

  const int c = (w * 3 + 14) / 28;   // 1/7th of 3/4 of width
  const int d = (w - c * 7 + 1) / 2; // remaining 1/8th of width

  const int p4 = (3 * h + 6) / 12; // 3/12th of height
  const int p23 = (h + 6) / 12;    // 1/12th of height
  const int p1 = h - p23 * 2 - p4; // remaining 7/12th of height

  /*
  //               75%  Rec709 -- Grey40 Grey75 Yellow  Cyan   Green Magenta  Red   Blue
  static const uint8_t pattern1Y[] = { 104,   180,   168,   145,   133,    63,    51,    28 };
  static const uint8_t pattern1U[] = { 128,   128,    44,   147,    63,   193,   109,   212 };
  static const uint8_t pattern1V[] = { 128,   128,   136,    44,    52,   204,   212,   120 };
  // 3.7.6: replaced the 8 bit table (inaccurate base for higher bitdepths) with accurate
  // RGB values with double precision RGB to YUV conversion.
  */

  // Define as double-precision gamma-encoded signal levels (E'R, E'G, E'B).
  // 0.75 = 75% of encoded signal swing, not a linear-light value.
  // Convert to Rec.709 YUV for target bitdepth and limited/full range later.
  static const double pattern1R[] = {0.4, 0.75, 0.75, 0.00, 0.00, 0.75, 0.75, 0.00};
  static const double pattern1G[] = {0.4, 0.75, 0.75, 0.75, 0.75, 0.00, 0.00, 0.00};
  static const double pattern1B[] = {0.4, 0.75, 0.00, 0.75, 0.00, 0.75, 0.00, 0.75};

  // Helper to process and write a pixel based on RGB input
  auto ProcessPixel = [&](double r, double g, double b, int targetX) {
    double dY, dU, dV;
    GetYUVRec709fromRGB(r, g, b, dY, dU, dV);

    if constexpr (std::is_same<pixel_t, float>::value) {
      pY[targetX] = (pixel_t)(dY * float_scale + float_offset);
      pU[targetX] = (pixel_t)(dU * float_uv_scale);
      pV[targetX] = (pixel_t)(dV * float_uv_scale);
    } else {
      // High-precision calculation for 10/12/16-bit
      pY[targetX] = (pixel_t)(((dY * 219.0 + 16.0) * (1 << shift)) + 0.5);
      pU[targetX] = (pixel_t)(((dU * 224.0 + 128.0) * (1 << shift)) + 0.5);
      pV[targetX] = (pixel_t)(((dV * 224.0 + 128.0) * (1 << shift)) + 0.5);
    }
  };

  // ColorbarsHD produces "limited", and since Avisynth handles "limited" 32 bit float, so we adjust it as well.

  // Pattern 1

  {
    const int first_y = y;
    const auto *firstY = pY, *firstU = pU, *firstV = pV;
    for (; y < p1 && y < h; ++y) {
      if (copy && y != first_y) {
        for (int row = 0; row < 1; ++row)
          copy(reinterpret_cast<uint8_t*>(pY + row * pitchY), reinterpret_cast<const uint8_t*>(firstY),
               w * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pU), reinterpret_cast<const uint8_t*>(firstU), w * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pV), reinterpret_cast<const uint8_t*>(firstV), w * sizeof(pixel_t));
        pY += pitchY * 1;
        pU += pitchUV;
        pV += pitchUV;
        continue;
      }

      int x = 0;
      // 40% grey
      for (; x < d && x < w; ++x) {
        ProcessPixel(pattern1R[0], pattern1G[0], pattern1B[0], x);
      }
      // 75% White, Yellow, Cyan, Green, Magenta, Red, Blue
      for (int i = 1; i < 8; i++) {
        for (int j = 0; j < c && x < w; ++j, ++x) {
          ProcessPixel(pattern1R[i], pattern1G[i], pattern1B[i], x);
        }
      }
      for (; x < w; ++x) {
        ProcessPixel(pattern1R[0], pattern1G[0], pattern1B[0], x);
      }
      pY += pitchY;
      pU += pitchUV;
      pV += pitchUV;
    }
  }

  /*
  SMPTE RP 219 / EG 1: +I Signal Reference (Rec. 709 / HD)
  * The +I (In-phase) signal is defined by its analog IRE levels:
    R = 41.2545 IRE, G = 16.6946 IRE, B = 0 IRE.
  * Normalized Linear RGB (IRE/100): R: 0.412545, G: 0.166946, B: 0.000000
  -----------------------------------------------------------
  Bit-Depth | Y (Luma)      | U (Cb)        | V (Cr)        |
  -----------------------------------------------------------
  8-bit     | 61   (3D)     | 103  (67)     | 157  (9D)     |
  10-bit    | 245  (0F5)    | 412  (19C)    | 629  (275)    |
  16-bit    | 15707 (3D5B)  | 26368 (6700)  | 40249 (9D39)  |
  -----------------------------------------------------------
  See also https://www.arib.or.jp/english/html/overview/doc/6-STD-B28v1_0-E1.pdf
  and
  Wikipedia (https://en.wikipedia.org/wiki/SMPTE_color_bars) (2026)

  Pre 3.7.6 old table, containing precalculated 8 bit values
  //              100% Rec709       Cyan  Blue Yellow  Red    +I Grey75  White
  static const uint8_t pattern23Y[] = { 188,   32,  219,   63,   61,  180,  235 };
  static const uint8_t pattern23U[] = { 154,  240,   16,  102,  103,  128,  128 };
  static const uint8_t pattern23V[] = {  16,  118,  138,  240,  157,  128,  128 };

  */
  // Pattern 2

  // 0: Cyan100, 1: Blue100, 2: Yellow100, 3: Red100, 4: +I, 5: Grey75, 6: White100
  static const double pattern23R[] = {0.0, 0.0, 1.0, 1.0, PLUS_I_R_YUV, 0.75, 1.0};
  static const double pattern23G[] = {1.0, 0.0, 1.0, 0.0, PLUS_I_G_YUV, 0.75, 1.0};
  static const double pattern23B[] = {1.0, 1.0, 0.0, 0.0, PLUS_I_B_YUV, 0.75, 1.0};

  // For pattern 2 and 3
  auto ProcessBar = [&](int index, int endX, int& currentX) {
    double dY, dU, dV;
    GetYUVRec709fromRGB(pattern23R[index], pattern23G[index], pattern23B[index], dY, dU, dV);

    for (; currentX < endX && currentX < w; ++currentX) {
      if constexpr (std::is_same<pixel_t, float>::value) {
        pY[currentX] = (pixel_t)(dY * float_scale + float_offset);
        pU[currentX] = (pixel_t)(dU * float_uv_scale);
        pV[currentX] = (pixel_t)(dV * float_uv_scale);
      } else {
        pY[currentX] = (pixel_t)(((dY * 219.0 + 16.0) * (1 << shift)) + 0.5);
        pU[currentX] = (pixel_t)(((dU * 224.0 + 128.0) * (1 << shift)) + 0.5);
        pV[currentX] = (pixel_t)(((dV * 224.0 + 128.0) * (1 << shift)) + 0.5);
      }
    }
  };

  {
    const int first_y = y;
    const auto *firstY = pY, *firstU = pU, *firstV = pV;
    for (; y < p1 + p23 && y < h; ++y) {
      if (copy && y != first_y) {
        for (int row = 0; row < 1; ++row)
          copy(reinterpret_cast<uint8_t*>(pY + row * pitchY), reinterpret_cast<const uint8_t*>(firstY),
               w * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pU), reinterpret_cast<const uint8_t*>(firstU), w * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pV), reinterpret_cast<const uint8_t*>(firstV), w * sizeof(pixel_t));
        pY += pitchY * 1;
        pU += pitchUV;
        pV += pitchUV;
        continue;
      }

      int x = 0;

      // 1. Left Padding (100% Cyan) - Index 0
      ProcessBar(0, d, x);
      // 2. The +I Bar - Index 4 (+I or Grey75 or White)
      ProcessBar(4, c + d, x);
      // 3. 75% White (Grey75) - Index 5
      ProcessBar(5, c * 7 + d, x);
      // 4. Remaining width (100% Blue) - Index 1
      ProcessBar(1, w, x);

      pY += pitchY;
      pU += pitchUV;
      pV += pitchUV;
    }
  }

  // Pattern 3

  {
    const int first_y = y;
    const auto *firstY = pY, *firstU = pU, *firstV = pV;
    for (; y < p1 + p23 * 2 && y < h; ++y) {
      if (copy && y != first_y) {
        for (int row = 0; row < 1; ++row)
          copy(reinterpret_cast<uint8_t*>(pY + row * pitchY), reinterpret_cast<const uint8_t*>(firstY),
               w * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pU), reinterpret_cast<const uint8_t*>(firstU), w * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pV), reinterpret_cast<const uint8_t*>(firstV), w * sizeof(pixel_t));
        pY += pitchY * 1;
        pU += pitchUV;
        pV += pitchUV;
        continue;
      }

      int x = 0;

      ProcessBar(2, d, x); // 100% Yellow

      // Y ramp section: 0% Black, Ramp, 100% White
      // FIXED in 3.7.6: Y-ramp to conform SMPTE RP 219-1:2014, put 0% Black before and 100% White after
      // Divide the c * 7 area:
      // - 1x - 0% Black (or optional +Q)
      // - 5x - Ramp
      // - 1x - 100% White

      int rampStartX = x + c;        // End of Black (+Q) step
      int rampEndX = x + (c * 6);    // Start of White step
      int sectionEndX = x + (c * 7); // End of this whole middle section

      // A. 0% black (or optional +Q) step
      for (; x < rampStartX && x < w; ++x) {
        ProcessPixel(0.0, 0.0, 0.0, x);
        // ProcessPixel(PLUS_Q_R_YUV, PLUS_Q_G_YUV, PLUS_Q_B_YUV, x); // For +Q instead of 0% Black
      }

      // B. Y-ramp (0.0 to 1.0) - 5 units wide
      int rampWidth = rampEndX - rampStartX;
      for (int j = 0; x < rampEndX && x < w; ++x, ++j) {
        double v = (double)j / (rampWidth - 1);
        ProcessPixel(v, v, v, x); // For a grayscale ramp, R=G=B
      }

      // C. 100% White
      for (; x < sectionEndX && x < w; ++x) {
        ProcessPixel(1.0, 1.0, 1.0, x);
      }
      // end of Ramp section

      ProcessBar(3, w, x); // 100% Red
      pY += pitchY;
      pU += pitchUV;
      pV += pitchUV;
    }
  }

  // Pattern 4

  // Normalized RGB for Pattern 4: 15% Grey, Black, White, Black, -2%, Black, +2%, Black, +4%, Black
  /* old table, precalculated 8 bit values
  //                             Grey15 Black White Black   -2% Black   +2% Black   +4% Black
  static const uint8_t pattern4Y[] = { 49,   16,  235,   16,   12,   16,   20,   16,   25,   16 };
  U and V are 128
  */

  static const double pattern4RGB[] = {0.15, 0.0, 1.0, 0.0, -0.02, 0.0, 0.02, 0.0, 0.04, 0.0};
  static const uint8_t pattern4W[] = {0, 9, 21, 26, 28, 30, 32, 34, 36, 42}; // in 6th's
  {
    const int first_y = y;
    const auto *firstY = pY, *firstU = pU, *firstV = pV;
    for (; y < h; ++y) {
      if (copy && y != first_y) {
        for (int row = 0; row < 1; ++row)
          copy(reinterpret_cast<uint8_t*>(pY + row * pitchY), reinterpret_cast<const uint8_t*>(firstY),
               w * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pU), reinterpret_cast<const uint8_t*>(firstU), w * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pV), reinterpret_cast<const uint8_t*>(firstV), w * sizeof(pixel_t));
        pY += pitchY * 1;
        pU += pitchUV;
        pV += pitchUV;
        continue;
      }

      int x = 0;

      // 1. Left Padding (15% Grey)
      for (; x < d && x < w; ++x) {
        ProcessPixel(pattern4RGB[0], pattern4RGB[0], pattern4RGB[0], x);
      }

      // 2. PLUGE and Bars (Indices 1 through 9)
      for (int i = 1; i <= 9; i++) {
        int endX = d + (pattern4W[i] * c + 3) / 6;
        for (; x < endX && x < w; ++x) {
          ProcessPixel(pattern4RGB[i], pattern4RGB[i], pattern4RGB[i], x);
        }
      }

      // 3. Right Padding (15% Grey)
      for (; x < w; ++x) {
        ProcessPixel(pattern4RGB[0], pattern4RGB[0], pattern4RGB[0], x);
      }

      pY += pitchY;
      pU += pitchUV;
      pV += pitchUV;
    }
  }
} // ColorBarsHD

/*******************************************************************
*
* ColorBars for YUV formats (BT.601) and and RGB
*
*********************************************************************/
// See also: http://avisynth.nl/index.php/ColorBars_theory
// and https://avisynthplus.readthedocs.io/en/latest/avisynthdoc/corefilters/colorbars.html

static const double top_two_thirdsR[] = {0.75, 0.75, 0.0, 0.0, 0.75, 0.75, 0.0};
static const double top_two_thirdsG[] = {0.75, 0.75, 0.75, 0.75, 0.0, 0.0, 0.0};
static const double top_two_thirdsB[] = {0.75, 0.0, 0.75, 0.0, 0.75, 0.0, 0.75};

// 2/3 to 3/4: Blue, Black, Magenta, Black, Cyan, Black, LtGrey
static const double two_thirds_to_three_quartersR[] = {0.0, 0.0, 0.75, 0.0, 0.0, 0.0, 0.75};
static const double two_thirds_to_three_quartersG[] = {0.0, 0.0, 0.0, 0.0, 0.75, 0.0, 0.75};
static const double two_thirds_to_three_quartersB[] = {0.75, 0.0, 0.75, 0.0, 0.75, 0.0, 0.75};

/*
   BOTTOM QUARTER BAR DEFINITIONS (-I, White, +Q, Black, -4%, Black, +4%, Black)

   ============================================================================
   DERIVATION OF -I AND +Q VALUES
   ============================================================================

   The -I and +Q signals are defined in the YIQ colorspace as pure chroma-axis
   signals with zero luma and 20 IRE saturation (0.2162 normalized):

     -I:  I = -0.2162,  Q = 0
     +Q:  I = 0,        Q = +0.2162

   Converting via the BT.601 UV rotation (Poynton eq. 33, with UV swap):

     -I raw RGB (Y=0):  R = -0.2067,  G = +0.0588,  B = +0.2394
     +Q raw RGB (Y=0):  R = +0.1343,  G = -0.1400,  B = +0.3685

   Both signals contain out-of-range (negative) RGB components. Three
   interpretations exist in the literature:

   ---------------------------------------------------------------------------
   OPTION 1 — Zero-luma, lift to studio black (legacy YUV implementation)
   ---------------------------------------------------------------------------
   Y = 16 (studio black). The most negative RGB component is left negative
   and will encode to a super-black code (0-15 range), or must be clipped
   when rendering as RGB, distorting the color.

   This is a HACK that had to be applied differently for RGB vs YUV:

   For YUV output (bottom_quarterR/G/B_for_YUV):
     No lift applied - use raw zero-luma RGB values
     Result: Y = 16, perfectly preserved chroma-axis definition
     Consequence: RGB contains super-black components (codes < 16)

   For RGB output (legacy AviSynth approach, NOT current implementation):
     Each component individually clamped/adjusted to avoid codes < 16
     Result: RGB codes all ≥ 16, but YUV back-conversion doesn't match
     Consequence: Loss of theoretical purity, inconsistent RGB↔YUV round-trip

   This was the legacy AviSynth ColorBars implementation:
     RGB path used bitmap-derived values:  -I = RGB(16, 70, 106)
     YUV path used zero-luma calculation:  -I = Y16 Cb158 Cr95
     These two specifications are fundamentally incompatible.

   ---------------------------------------------------------------------------
   OPTION 2 — Luma-corrected to studio black (CURRENT RGB IMPLEMENTATION)
   ---------------------------------------------------------------------------
   Luma is raised until the most negative component reaches code 16
   (studio black). The lift calculation:

     For -I: Y_lift = 0.2067 - 16/219 = 0.13364
     For +Q: Y_lift = 0.1400 - 16/219 = 0.06694

   After lifting (bottom_quarterR/G/B arrays, RGB-native):
     -I: R = 16 (studio black), G = 90, B = 130, Y ≈ 77
     +Q: R = 92, G = 16 (studio black), B = 143, Y ≈ 63

   This gives a consistent, broadcast-safe signal for RGB output with all
   components within valid studio range (16-235), but produces different
   YUV values than the legacy zero-luma specification (Y=77/63 vs Y=16).

   We maintain TWO separate ground truth tables to preserve legacy compatibility:

   1. bottom_quarterR/G/B (Option 2):
      - Used for RGB output formats
      - all I and Q codes >= 16
      - Colorimetrically consistent RGB↔YUV conversion

   2. bottom_quarterR/G/B_for_YUV (Option 1 YUV side):
      - Used for YUV output formats
      - Produces exact legacy values: -I Y=16, +Q Y=16
      - Contains out-of-range RGB components (will clip if rendered as RGB)

   This dual-table approach acknowledges the historical reality: the original
   AviSynth ColorBars had two independent specifications that don't convert
   to each other via standard matrix math. The -I and +Q signals were analog
   broadcast test signals (voltage levels), not digital RGB/YUV values, and
   their digital representation requires compromises.

   So we use different linear RGB tables for -I and +Q bars, depending on whether
   the target is RGB or YUV, to best match legacy values in each domain.
   Note that moving I and Q values to provide legal RGB and YUV values is a "hack".
   These values match the legacy Avisynth.
*/

// ===== RGB-NATIVE GROUND TRUTH =====
// For RGB output formats only.
// Luma-corrected: most negative component lifted to studio black (code 16).
// -I: R lifted to code 16  ->  RGB(16, 90, 130) at 8-bit
// +Q: G lifted to code 16  ->  RGB(92, 16, 143) at 8-bit
// Convention: 0.0 = code 16 (studio black), 1.0 = code 235 (studio white)
static const double bottom_quarterR[] = {MINUS_I_R, 1.0, PLUS_Q_R, 0.0, -0.04, 0.0, 0.04, 0.0};
static const double bottom_quarterG[] = {MINUS_I_G, 1.0, PLUS_Q_G, 0.0, -0.04, 0.0, 0.04, 0.0};
static const double bottom_quarterB[] = {MINUS_I_B, 1.0, PLUS_Q_B, 0.0, -0.04, 0.0, 0.04, 0.0};

// ===== YUV-TARGETED RGB GROUND TRUTH =====
// For YUV output formats only.
// When converted via GetYUVBT601fromRGB, produces legacy YUV values:
// -I: Y=16, Cb=158, Cr=95   (zero-luma, pure chroma definition)
// +Q: Y=16, Cb=174, Cr=149  (zero-luma, pure chroma definition)
// Note: Contains out-of-range values (R < 0 for -I, G < 0 for +Q)
//       which will be clamped when rendering RGB formats.
// Convention: 0.0 = code 16 (studio black), 1.0 = code 235 (studio white)
static const double bottom_quarterR_for_YUV[] = {MINUS_I_R_YUV, 1.0, PLUS_Q_R_YUV, 0.0, -0.04, 0.0, 0.04, 0.0};
static const double bottom_quarterG_for_YUV[] = {MINUS_I_G_YUV, 1.0, PLUS_Q_G_YUV, 0.0, -0.04, 0.0, 0.04, 0.0};
static const double bottom_quarterB_for_YUV[] = {MINUS_I_B_YUV, 1.0, PLUS_Q_B_YUV, 0.0, -0.04, 0.0, 0.04, 0.0};

/*******************************************************************
* ColorBars for YUV
*********************************************************************/

static void GetYUVBT601fromRGB(double R, double G, double B, double& dY, double& dU, double& dV) {
  // See https://www.itu.int/rec/R-REC-BT.601/en
  double Kr, Kb;
  Kr = 0.299;
  Kb = 0.114; // BT601: Kr=0.299, Kb=0.114
  dY = Kr * R + (1.0 - Kr - Kb) * G + Kb * B;
  dU = (B - dY) / (2.0 * (1.0 - Kb));
  dV = (R - dY) / (2.0 * (1.0 - Kr));
}

// BT.601 YUV conversion constants for ColorBars (Rec. ITU-R BT.801-1)
// Ground truth linear RGB -> BT.601 YUV, integer and float limited range output.
// Replaces the old hardcoded 8-bit tables.

// Bar boundaries are computed in chroma coordinates so that color transitions
// always fall on chroma-aligned luma positions (a multiple of the horizontal
// subsampling factor). This satisfies the requirement from Rec. ITU-R BT.801-1
// that transitions occur on chroma-aligned boundaries.
// Note: due to integer rounding, boundary positions may differ by +/-1 luma pixel
// compared to a 4:4:4 or RGB rendering of the same width, which is unavoidable
// when 7 bars do not divide evenly into the frame width.
template <typename pixel_t, bool is420, bool is422, bool is411>
static void draw_colorbars_yuv(uint8_t* pY8, uint8_t* pU8, uint8_t* pV8, int pitchY, int pitchUV, int w, int h,
                               int bits_per_pixel, Copy copy) {
  pixel_t* pY = reinterpret_cast<pixel_t*>(pY8);
  pixel_t* pU = reinterpret_cast<pixel_t*>(pU8);
  pixel_t* pV = reinterpret_cast<pixel_t*>(pV8);
  pitchY /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

  const int shift = sizeof(pixel_t) == 4 ? 0 : (bits_per_pixel - 8);

  // Pre-compute conversion constants for float limited range,
  // using the same centralized function as ColorbarsHD.

  // Also for float target we make "limited" range
  // Match upstream float rounding before promotion to double.
  constexpr FloatRange luma{219.0f / 255.0f, 16.0f / 255.0f};
  constexpr FloatRange chroma{(112.0f / 255.0f) / 0.5f, 0.0f};
  double float_offset = luma.dst_offset;
  double float_scale = luma.mul_factor; // 219.0 / 255.0;
  double float_uv_scale = chroma.mul_factor;

  // Convert one RGB triplet to a YUV pixel triplet at target bit depth.
  struct YUV3 {
    pixel_t y, u, v;
  };

  // Helper to process and write a pixel based on RGB input
  // Convention: encoded signal levels; 0.0 = code 16 (studio black), 1.0 = code 235 (studio white)

  auto make_yuv = [&](double r, double g, double b) -> YUV3 {
    double dY, dU, dV;
    GetYUVBT601fromRGB(r, g, b, dY, dU, dV);

    if constexpr (std::is_same<pixel_t, float>::value) {
      return {(pixel_t)(dY * float_scale + float_offset), (pixel_t)(dU * float_uv_scale),
              (pixel_t)(dV * float_uv_scale)};
    } else {
      // High-precision calculation for 10/12/16-bit
      return {(pixel_t)(((dY * 219.0 + 16.0) * (1 << shift)) + 0.5),
              (pixel_t)(((dU * 224.0 + 128.0) * (1 << shift)) + 0.5),
              (pixel_t)(((dV * 224.0 + 128.0) * (1 << shift)) + 0.5)};
    }
  };

  // Pre-compute all bar entries from ground truth RGB tables.
  YUV3 bq[8], ttq[7], ttt[7];
  for (int i = 0; i < 8; ++i)
    bq[i] = make_yuv(bottom_quarterR_for_YUV[i], bottom_quarterG_for_YUV[i], bottom_quarterB_for_YUV[i]);
  for (int i = 0; i < 7; ++i)
    ttq[i] =
        make_yuv(two_thirds_to_three_quartersR[i], two_thirds_to_three_quartersG[i], two_thirds_to_three_quartersB[i]);
  for (int i = 0; i < 7; ++i)
    ttt[i] = make_yuv(top_two_thirdsR[i], top_two_thirdsG[i], top_two_thirdsB[i]);

  // Write luma for one chroma-sample position x.
  // For subsampled formats, each chroma x covers multiple luma pixels.
  // For 444, chromaX == lumaX directly.
  // MSVC v141 misses implicit captures used only in if constexpr branches.
  auto write_luma = [&pY, &pitchY](int x, pixel_t yval) {
    if constexpr (is420)
      pY[x * 2 + 0] = pY[x * 2 + 1] = pY[x * 2 + pitchY] = pY[x * 2 + 1 + pitchY] = yval;
    else if constexpr (is422)
      pY[x * 2 + 0] = pY[x * 2 + 1] = yval;
    else if constexpr (is411)
      pY[x * 4 + 0] = pY[x * 4 + 1] = pY[x * 4 + 2] = pY[x * 4 + 3] = yval;
    else // 444
      pY[x] = yval;
  };

  auto write_yuv = [&](int x, const YUV3& c) {
    write_luma(x, c.y);
    pU[x] = c.u;
    pV[x] = c.v;
  };

  // For subsampled formats the chroma plane is narrower/shorter.
  // We iterate in chroma coordinates; write_luma expands to luma coordinates.
  int wUV = w;
  int hUV = h;
  if constexpr (is420 || is422)
    wUV >>= 1;
  if constexpr (is411)
    wUV >>= 2;
  if constexpr (is420)
    hUV >>= 1;

  int y = 0;

  // Top 2/3
  {
    const int first_y = y;
    const auto *firstY = pY, *firstU = pU, *firstV = pV;
    for (; y * 3 < hUV * 2; ++y) {
      if (copy && y != first_y) {
        for (int row = 0; row < (is420 ? 2 : 1); ++row)
          copy(reinterpret_cast<uint8_t*>(pY + row * pitchY), reinterpret_cast<const uint8_t*>(firstY),
               w * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pU), reinterpret_cast<const uint8_t*>(firstU), wUV * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pV), reinterpret_cast<const uint8_t*>(firstV), wUV * sizeof(pixel_t));
        pY += pitchY * (is420 ? 2 : 1);
        pU += pitchUV;
        pV += pitchUV;
        continue;
      }

      int x = 0;
      for (int i = 0; i < 7; ++i)
        for (; x < (wUV * (i + 1) + 3) / 7; ++x)
          write_yuv(x, ttt[i]);
      if constexpr (is420)
        pY += pitchY * 2;
      else
        pY += pitchY;
      pU += pitchUV;
      pV += pitchUV;
    }
  }

  // Middle band (2/3 to 3/4)
  {
    const int first_y = y;
    const auto *firstY = pY, *firstU = pU, *firstV = pV;
    for (; y * 4 < hUV * 3; ++y) {
      if (copy && y != first_y) {
        for (int row = 0; row < (is420 ? 2 : 1); ++row)
          copy(reinterpret_cast<uint8_t*>(pY + row * pitchY), reinterpret_cast<const uint8_t*>(firstY),
               w * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pU), reinterpret_cast<const uint8_t*>(firstU), wUV * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pV), reinterpret_cast<const uint8_t*>(firstV), wUV * sizeof(pixel_t));
        pY += pitchY * (is420 ? 2 : 1);
        pU += pitchUV;
        pV += pitchUV;
        continue;
      }

      int x = 0;
      for (int i = 0; i < 7; ++i)
        for (; x < (wUV * (i + 1) + 3) / 7; ++x)
          write_yuv(x, ttq[i]);
      if constexpr (is420)
        pY += pitchY * 2;
      else
        pY += pitchY;
      pU += pitchUV;
      pV += pitchUV;
    }
  }

  // Bottom quarter
  {
    const int first_y = y;
    const auto *firstY = pY, *firstU = pU, *firstV = pV;
    for (; y < hUV; ++y) {
      if (copy && y != first_y) {
        for (int row = 0; row < (is420 ? 2 : 1); ++row)
          copy(reinterpret_cast<uint8_t*>(pY + row * pitchY), reinterpret_cast<const uint8_t*>(firstY),
               w * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pU), reinterpret_cast<const uint8_t*>(firstU), wUV * sizeof(pixel_t));
        copy(reinterpret_cast<uint8_t*>(pV), reinterpret_cast<const uint8_t*>(firstV), wUV * sizeof(pixel_t));
        pY += pitchY * (is420 ? 2 : 1);
        pU += pitchUV;
        pV += pitchUV;
        continue;
      }

      int x = 0;
      for (int i = 0; i < 4; ++i)
        for (; x < (wUV * (i + 1) * 5 + 14) / 28; ++x)
          write_yuv(x, bq[i]);
      for (int j = 4; j < 7; ++j)
        for (; x < (wUV * (j + 12) + 10) / 21; ++x)
          write_yuv(x, bq[j]);
      for (; x < wUV; ++x)
        write_yuv(x, bq[7]);
      if constexpr (is420)
        pY += pitchY * 2;
      else
        pY += pitchY;
      pU += pitchUV;
      pV += pitchUV;
    }
  }
}

/*******************************************************************
* ColorBars for RGB (packed 32/64, 24/48, planar RGB)
*********************************************************************/

// Convert normalised linear RGB [0.0..1.0] to integer studio/limited RGB at any bit depth.
// Limited range: black = 16 << (bpp-8), white = 235 << (bpp-8)
// Values outside [0.0..1.0] (e.g. PLUGE -4%, +4%) are handled naturally.
static int studio_rgb_to_integer(double value, int bits_per_pixel) {
  const int offset = 16 << (bits_per_pixel - 8);
  const int range = 219 << (bits_per_pixel - 8);
  return (int)(value * range + offset + 0.5);
}

template <typename pixel_t>
static void draw_colorbars_rgb3264(uint8_t* p8, int pitch, int w, int h, Copy copy) {
  typedef typename std::conditional<sizeof(pixel_t) == 2, uint64_t, uint32_t>::type internal_pixel_t;
  internal_pixel_t* p = reinterpret_cast<internal_pixel_t*>(p8);
  pitch /= sizeof(pixel_t);

  // Pre-compute packed pixel values from ground truth double tables at target bit depth.
  // Pack order in uint32: 0x00RRGGBB, in uint64: RR(16)GG(16)BB(16) (alpha/padding zero)
  // RGB32/64 pixel layout (bottom byte = B, then G, then R, then pad/alpha)
  auto make_pixel = [&](double r, double g, double b) -> internal_pixel_t {
    if constexpr (sizeof(pixel_t) == 1) {
      // RGB32: 8-bit per channel, packed as 0x00RRGGBB
      uint32_t ri = (uint32_t)studio_rgb_to_integer(r, 8);
      uint32_t gi = (uint32_t)studio_rgb_to_integer(g, 8);
      uint32_t bi = (uint32_t)studio_rgb_to_integer(b, 8);
      return (internal_pixel_t)((ri << 16) | (gi << 8) | bi);
    } else {
      // RGB64: 16-bit per channel
      uint64_t ri = (uint64_t)studio_rgb_to_integer(r, 16);
      uint64_t gi = (uint64_t)studio_rgb_to_integer(g, 16);
      uint64_t bi = (uint64_t)studio_rgb_to_integer(b, 16);
      return (internal_pixel_t)((ri << 32) | (gi << 16) | bi);
    }
  };

  // Pre-compute all entries (bottom->top scan order matches original)
  internal_pixel_t bq[8], ttq[7], ttt[7];
  for (int i = 0; i < 8; ++i)
    bq[i] = make_pixel(bottom_quarterR[i], bottom_quarterG[i], bottom_quarterB[i]);
  for (int i = 0; i < 7; ++i)
    ttq[i] = make_pixel(two_thirds_to_three_quartersR[i], two_thirds_to_three_quartersG[i],
                        two_thirds_to_three_quartersB[i]);
  for (int i = 0; i < 7; ++i)
    ttt[i] = make_pixel(top_two_thirdsR[i], top_two_thirdsG[i], top_two_thirdsB[i]);

  // note we go bottom->top
  int y = 0;
  {
    const int first_y = y;
    const auto* firstP = p;
    for (; y < h / 4; ++y) {
      if (copy && y != first_y) {
        copy(reinterpret_cast<uint8_t*>(p), reinterpret_cast<const uint8_t*>(firstP), w * sizeof(internal_pixel_t));
        p += pitch;
        continue;
      }

      int x = 0;
      for (int i = 0; i < 4; ++i)
        for (; x < (w * (i + 1) * 5 + 14) / 28; ++x)
          p[x] = bq[i];
      for (int j = 4; j < 7; ++j)
        for (; x < (w * (j + 12) + 10) / 21; ++x)
          p[x] = bq[j];
      for (; x < w; ++x)
        p[x] = bq[7];
      p += pitch;
    }
  }
  {
    const int first_y = y;
    const auto* firstP = p;
    for (; y < h / 3; ++y) {
      if (copy && y != first_y) {
        copy(reinterpret_cast<uint8_t*>(p), reinterpret_cast<const uint8_t*>(firstP), w * sizeof(internal_pixel_t));
        p += pitch;
        continue;
      }

      int x = 0;
      for (int i = 0; i < 7; ++i)
        for (; x < (w * (i + 1) + 3) / 7; ++x)
          p[x] = ttq[i];
      p += pitch;
    }
  }
  {
    const int first_y = y;
    const auto* firstP = p;
    for (; y < h; ++y) {
      if (copy && y != first_y) {
        copy(reinterpret_cast<uint8_t*>(p), reinterpret_cast<const uint8_t*>(firstP), w * sizeof(internal_pixel_t));
        p += pitch;
        continue;
      }

      int x = 0;
      for (int i = 0; i < 7; ++i)
        for (; x < (w * (i + 1) + 3) / 7; ++x)
          p[x] = ttt[i];
      p += pitch;
    }
  }
}

template <typename pixel_t>
static void draw_colorbars_rgb2448(uint8_t* p8, int pitch, int w, int h, Copy copy) {
  pixel_t* p = reinterpret_cast<pixel_t*>(p8);
  pitch /= sizeof(pixel_t);

  // Pre-computed triplets from ground truth double tables
  struct RGB3 {
    pixel_t r, g, b;
  };

  auto make_rgb3 = [&](double r, double g, double b) -> RGB3 {
    return {(pixel_t)studio_rgb_to_integer(r, sizeof(pixel_t) == 1 ? 8 : 16),
            (pixel_t)studio_rgb_to_integer(g, sizeof(pixel_t) == 1 ? 8 : 16),
            (pixel_t)studio_rgb_to_integer(b, sizeof(pixel_t) == 1 ? 8 : 16)};
  };

  // Pre-compute all entries (bottom->top scan order matches original)
  RGB3 bq[8], ttq[7], ttt[7];
  for (int i = 0; i < 8; ++i)
    bq[i] = make_rgb3(bottom_quarterR[i], bottom_quarterG[i], bottom_quarterB[i]);
  for (int i = 0; i < 7; ++i)
    ttq[i] =
        make_rgb3(two_thirds_to_three_quartersR[i], two_thirds_to_three_quartersG[i], two_thirds_to_three_quartersB[i]);
  for (int i = 0; i < 7; ++i)
    ttt[i] = make_rgb3(top_two_thirdsR[i], top_two_thirdsG[i], top_two_thirdsB[i]);

  auto write_pixel = [&](int x, const RGB3& c) {
    p[x * 3 + 0] = c.b; // RGB24/48 memory layout: B, G, R
    p[x * 3 + 1] = c.g;
    p[x * 3 + 2] = c.r;
  };

  // note we go bottom->top
  int y = 0;
  {
    const int first_y = y;
    const auto* firstP = p;
    for (; y < h / 4; ++y) {
      if (copy && y != first_y) {
        copy(reinterpret_cast<uint8_t*>(p), reinterpret_cast<const uint8_t*>(firstP), w * 3 * sizeof(pixel_t));
        p += pitch;
        continue;
      }

      int x = 0;
      for (int i = 0; i < 4; ++i)
        for (; x < (w * (i + 1) * 5 + 14) / 28; ++x)
          write_pixel(x, bq[i]);
      for (int j = 4; j < 7; ++j)
        for (; x < (w * (j + 12) + 10) / 21; ++x)
          write_pixel(x, bq[j]);
      for (; x < w; ++x)
        write_pixel(x, bq[7]);
      p += pitch;
    }
  }
  {
    const int first_y = y;
    const auto* firstP = p;
    for (; y < h / 3; ++y) {
      if (copy && y != first_y) {
        copy(reinterpret_cast<uint8_t*>(p), reinterpret_cast<const uint8_t*>(firstP), w * 3 * sizeof(pixel_t));
        p += pitch;
        continue;
      }

      int x = 0;
      for (int i = 0; i < 7; ++i)
        for (; x < (w * (i + 1) + 3) / 7; ++x)
          write_pixel(x, ttq[i]);
      p += pitch;
    }
  }
  {
    const int first_y = y;
    const auto* firstP = p;
    for (; y < h; ++y) {
      if (copy && y != first_y) {
        copy(reinterpret_cast<uint8_t*>(p), reinterpret_cast<const uint8_t*>(firstP), w * 3 * sizeof(pixel_t));
        p += pitch;
        continue;
      }

      int x = 0;
      for (int i = 0; i < 7; ++i)
        for (; x < (w * (i + 1) + 3) / 7; ++x)
          write_pixel(x, ttt[i]);
      p += pitch;
    }
  }
}

template <typename pixel_t>
static void draw_colorbars_rgbp(uint8_t* pR8, uint8_t* pG8, uint8_t* pB8, int pitch, int w, int h, int bits_per_pixel,
                                Copy copy) {
  pixel_t* pR = reinterpret_cast<pixel_t*>(pR8);
  pixel_t* pG = reinterpret_cast<pixel_t*>(pG8);
  pixel_t* pB = reinterpret_cast<pixel_t*>(pB8);
  pitch /= sizeof(pixel_t);

  struct RGB3 {
    pixel_t r, g, b;
  };

  constexpr FloatRange rgb_luma_f{219.0f / 255.0f, 16.0f / 255.0f};

  auto make_rgb3 = [&](double r, double g, double b) -> RGB3 {
    if constexpr (std::is_same<pixel_t, float>::value) {
      return {(float)(r * rgb_luma_f.mul_factor + rgb_luma_f.dst_offset),
              (float)(g * rgb_luma_f.mul_factor + rgb_luma_f.dst_offset),
              (float)(b * rgb_luma_f.mul_factor + rgb_luma_f.dst_offset)};
    } else {
      return {(pixel_t)studio_rgb_to_integer(r, bits_per_pixel), (pixel_t)studio_rgb_to_integer(g, bits_per_pixel),
              (pixel_t)studio_rgb_to_integer(b, bits_per_pixel)};
    }
  };

  RGB3 bq[8], ttq[7], ttt[7];
  for (int i = 0; i < 8; ++i)
    bq[i] = make_rgb3(bottom_quarterR[i], bottom_quarterG[i], bottom_quarterB[i]);
  for (int i = 0; i < 7; ++i)
    ttq[i] =
        make_rgb3(two_thirds_to_three_quartersR[i], two_thirds_to_three_quartersG[i], two_thirds_to_three_quartersB[i]);
  for (int i = 0; i < 7; ++i)
    ttt[i] = make_rgb3(top_two_thirdsR[i], top_two_thirdsG[i], top_two_thirdsB[i]);

  auto write_pixel = [&](int x, const RGB3& c) {
    pR[x] = c.r;
    pG[x] = c.g;
    pB[x] = c.b;
  };

  // Planar RGB is top-to-bottom natively, no bottom-up workaround needed.
  // layout top->bottom: top_two_thirds, two_thirds_to_three_quarters, bottom_quarter
  int y = 0;

  // Top 2/3
  {
    const int first_y = y;
    const auto *firstR = pR, *firstG = pG, *firstB = pB;
    for (; y * 3 < h * 2; ++y) {
      if (copy && y != first_y) {
        copy(reinterpret_cast<uint8_t*>(pR), reinterpret_cast<const uint8_t*>(firstR), w * sizeof(pixel_t));
        pR += pitch;
        copy(reinterpret_cast<uint8_t*>(pG), reinterpret_cast<const uint8_t*>(firstG), w * sizeof(pixel_t));
        pG += pitch;
        copy(reinterpret_cast<uint8_t*>(pB), reinterpret_cast<const uint8_t*>(firstB), w * sizeof(pixel_t));
        pB += pitch;
        continue;
      }

      int x = 0;
      for (int i = 0; i < 7; ++i)
        for (; x < (w * (i + 1) + 3) / 7; ++x)
          write_pixel(x, ttt[i]);
      pR += pitch;
      pG += pitch;
      pB += pitch;
    }
  }

  // Middle band (2/3 to 3/4)
  {
    const int first_y = y;
    const auto *firstR = pR, *firstG = pG, *firstB = pB;
    for (; y * 4 < h * 3; ++y) {
      if (copy && y != first_y) {
        copy(reinterpret_cast<uint8_t*>(pR), reinterpret_cast<const uint8_t*>(firstR), w * sizeof(pixel_t));
        pR += pitch;
        copy(reinterpret_cast<uint8_t*>(pG), reinterpret_cast<const uint8_t*>(firstG), w * sizeof(pixel_t));
        pG += pitch;
        copy(reinterpret_cast<uint8_t*>(pB), reinterpret_cast<const uint8_t*>(firstB), w * sizeof(pixel_t));
        pB += pitch;
        continue;
      }

      int x = 0;
      for (int i = 0; i < 7; ++i)
        for (; x < (w * (i + 1) + 3) / 7; ++x)
          write_pixel(x, ttq[i]);
      pR += pitch;
      pG += pitch;
      pB += pitch;
    }
  }

  // Bottom quarter
  {
    const int first_y = y;
    const auto *firstR = pR, *firstG = pG, *firstB = pB;
    for (; y < h; ++y) {
      if (copy && y != first_y) {
        copy(reinterpret_cast<uint8_t*>(pR), reinterpret_cast<const uint8_t*>(firstR), w * sizeof(pixel_t));
        pR += pitch;
        copy(reinterpret_cast<uint8_t*>(pG), reinterpret_cast<const uint8_t*>(firstG), w * sizeof(pixel_t));
        pG += pitch;
        copy(reinterpret_cast<uint8_t*>(pB), reinterpret_cast<const uint8_t*>(firstB), w * sizeof(pixel_t));
        pB += pitch;
        continue;
      }

      int x = 0;
      for (int i = 0; i < 4; ++i)
        for (; x < (w * (i + 1) * 5 + 14) / 28; ++x)
          write_pixel(x, bq[i]);
      for (int j = 4; j < 7; ++j)
        for (; x < (w * (j + 12) + 10) / 21; ++x)
          write_pixel(x, bq[j]);
      for (; x < w; ++x)
        write_pixel(x, bq[7]);
      pR += pitch;
      pG += pitch;
      pB += pitch;
    }
  }
}

template <class T>
void planar_draw(uint8_t* const p[4], const int pitch[4], int w, int h, int bits, int layout, int hd, Copy copy) {
  if (hd)
    draw_colorbarsHD_444<T>(p[0], p[1], p[2], pitch[0], pitch[1], w, h, bits, copy);
  else
    switch (layout) {
      case 0:
        draw_colorbars_yuv<T, false, false, false>(p[0], p[1], p[2], pitch[0], pitch[1], w, h, bits, copy);
        break;
      case 1:
        draw_colorbars_yuv<T, false, true, false>(p[0], p[1], p[2], pitch[0], pitch[1], w, h, bits, copy);
        break;
      case 2:
        draw_colorbars_yuv<T, true, false, false>(p[0], p[1], p[2], pitch[0], pitch[1], w, h, bits, copy);
        break;
      case 3:
        draw_colorbars_yuv<T, false, false, true>(p[0], p[1], p[2], pitch[0], pitch[1], w, h, bits, copy);
        break;
      case 4:
        draw_colorbars_rgbp<T>(p[0], p[1], p[2], pitch[0], w, h, bits, copy);
        break;
    }
}
void draw(uint8_t* const p[4], const int pitch[4], int w, int h, int bits, int layout, int hd, const Kernels* kernels) {
  Copy copy = kernels ? kernels->copy : nullptr;
  // Frame-sized measurements favor upstream direct drawing over scanline copies
  // for these large formats. Keep Highway copies for smaller frames/other layouts.
  if (size_t(w) * h >= 1920u * 1080u && ((bits == 32 && (layout == 0 || layout == 4)) || (bits == 16 && layout == 6)))
    copy = nullptr;
  if (layout <= 4) {
    if (bits == 8)
      planar_draw<uint8_t>(p, pitch, w, h, bits, layout, hd, copy);
    else if (bits == 32)
      planar_draw<float>(p, pitch, w, h, bits, layout, hd, copy);
    else
      planar_draw<uint16_t>(p, pitch, w, h, bits, layout, hd, copy);
    if (p[3])
      for (int y = 0; y < h; ++y)
        std::memset(p[3] + ptrdiff_t(y) * pitch[3], 0, w * (bits == 8 ? 1 : bits == 32 ? 4 : 2));
  } else if (layout == 7) {
    std::vector<uint8_t> y(size_t(w) * h), u(size_t(w / 2) * h), v(u.size());
    draw_colorbars_yuv<uint8_t, false, true, false>(y.data(), u.data(), v.data(), w, w / 2, w, h, 8, copy);
    for (int row = 0; row < h; ++row) {
      if (kernels) {
        kernels->pack_yuy2(p[0] + ptrdiff_t(row) * pitch[0], y.data() + size_t(row) * w,
                           u.data() + size_t(row) * (w / 2), v.data() + size_t(row) * (w / 2), w);
      } else
        for (int x = 0; x < w / 2; ++x) {
          auto* dst = p[0] + ptrdiff_t(row) * pitch[0] + 4 * x;
          dst[0] = y[size_t(row) * w + 2 * x];
          dst[1] = u[size_t(row) * (w / 2) + x];
          dst[2] = y[size_t(row) * w + 2 * x + 1];
          dst[3] = v[size_t(row) * (w / 2) + x];
        }
    }
  } else if (layout == 5) {
    if (bits == 8)
      draw_colorbars_rgb2448<uint8_t>(p[0], pitch[0], w, h, copy);
    else
      draw_colorbars_rgb2448<uint16_t>(p[0], pitch[0], w, h, copy);
  } else {
    if (bits == 8)
      draw_colorbars_rgb3264<uint8_t>(p[0], pitch[0] / 4, w, h, copy);
    else
      draw_colorbars_rgb3264<uint16_t>(p[0], pitch[0] / 4, w, h, copy);
  }
}
} // namespace aif::color_bars
