// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from AviSynthPlus avs_core/filters/limiter.cpp.
#include "backend.h"
#include <limits>
using BYTE = uint8_t;
namespace {
constexpr float c8tof(int v) {
  return v / 255.0f;
}
constexpr float uv8tof(int v) {
  return (v - 128) / 255.0f;
}
enum { show_none, show_luma, show_luma_grey, show_chroma, show_chroma_grey };
template <typename pixel_t, bool show_luma_grey>
static void show_luma_with_grey_opt_yuv444(BYTE* srcp8, BYTE* srcpU8, BYTE* srcpV8, int pitch, int pitchUV, int width,
                                           int height, int min_luma, int max_luma, int bits_per_pixel) {
  // show_luma       Mark clamped pixels red/green over a colour image
  // show_luma_grey  Mark clamped pixels red/green over a greyscaled image
  const int shift = sizeof(pixel_t) == 1 ? 0 : (bits_per_pixel - 8);
  pixel_t* srcp = reinterpret_cast<pixel_t*>(srcp8);
  pixel_t* srcpU = reinterpret_cast<pixel_t*>(srcpU8);
  pixel_t* srcpV = reinterpret_cast<pixel_t*>(srcpV8);
  pitch /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

  for (int h = 0; h < height; h += 1) {
    for (int x = 0; x < width; x += 1) {
      if (srcp[x] < min_luma) {
        srcp[x] = 81 << shift;
        srcpU[x] = 91 << shift;
        srcpV[x] = 240 << shift;
      } // red:   Y=81, U=91 and V=240
      else if (srcp[x] > max_luma) {
        srcp[x] = 145 << shift;
        srcpU[x] = 54 << shift;
        srcpV[x] = 34 << shift;
      } // green: Y=145, U=54 and V=34
      // this differs from show_luma
      else if (show_luma_grey) {
        srcpU[x] = srcpV[x] = 128 << shift;
      } // grey
    }
    srcp += pitch;
    srcpV += pitchUV;
    srcpU += pitchUV;
  }
}

template <typename pixel_t, bool show_luma_grey>
static void show_luma_with_grey_opt_yuv420(BYTE* srcp8, BYTE* srcpU8, BYTE* srcpV8, int pitch, int pitchUV, int width,
                                           int height, int min_luma, int max_luma, int bits_per_pixel) {
  // show_luma       Mark clamped pixels red/green over a colour image
  // show_luma_grey  Mark clamped pixels red/green over a greyscaled image
  const int shift = sizeof(pixel_t) == 1 ? 0 : (bits_per_pixel - 8);
  pixel_t* srcp = reinterpret_cast<pixel_t*>(srcp8);
  pixel_t* srcn = reinterpret_cast<pixel_t*>(srcp8 + pitch); // next line
  pixel_t* srcpU = reinterpret_cast<pixel_t*>(srcpU8);
  pixel_t* srcpV = reinterpret_cast<pixel_t*>(srcpV8);
  pitch /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

  for (int h = 0; h < height; h += 2) {
    for (int x = 0; x < width; x += 2) {
      int uv = 0;
      if (srcp[x] < min_luma) {
        srcp[x] = 81 << shift;
        uv |= 1;
      } else if (srcp[x] > max_luma) {
        srcp[x] = 145 << shift;
        uv |= 2;
      }
      if (srcp[x + 1] < min_luma) {
        srcp[x + 1] = 81 << shift;
        uv |= 1;
      } else if (srcp[x + 1] > max_luma) {
        srcp[x + 1] = 145 << shift;
        uv |= 2;
      }
      if (srcn[x] < min_luma) {
        srcn[x] = 81 << shift;
        uv |= 1;
      } else if (srcn[x] > max_luma) {
        srcn[x] = 145 << shift;
        uv |= 2;
      }
      if (srcn[x + 1] < min_luma) {
        srcn[x + 1] = 81 << shift;
        uv |= 1;
      } else if (srcn[x + 1] > max_luma) {
        srcn[x + 1] = 145 << shift;
        uv |= 2;
      }
      switch (uv) {
        case 1:
          srcpU[x / 2] = 91 << shift;
          srcpV[x / 2] = 240 << shift;
          break; // red:   Y=81, U=91 and V=240
        case 2:
          srcpU[x / 2] = 54 << shift;
          srcpV[x / 2] = 34 << shift;
          break; // green: Y=145, U=54 and V=34
        // this differs from show_luma_grey
        case 3:
          if (show_luma_grey) {
            srcpU[x / 2] = 90 << shift;
            srcpV[x / 2] = 134 << shift;
            break; // puke:  Y=81, U=90 and V=134 olive: Y=145, U=90 and V=134
          } else {
            srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = 210 << shift; // yellow:Y=210, U=16 and V=146
            srcpU[x / 2] = 16 << shift;
            srcpV[x / 2] = 146 << shift;
          }
          break;
        default:
          if (show_luma_grey) {
            srcpU[x / 2] = srcpV[x / 2] = 128 << shift; // olive: Y=145, U=90 and V=134
          }
          break;
      }
    }
    srcp += pitch * 2; // 2x2 pixels at a time (4:2:0 subsampling)
    srcn += pitch * 2;
    srcpV += pitchUV;
    srcpU += pitchUV;
  }
}

template <bool show_luma_grey>
static void show_luma_with_grey_opt_yuv444_f(BYTE* srcp8, BYTE* srcpU8, BYTE* srcpV8, int pitch, int pitchUV, int width,
                                             int height, float min_luma, float max_luma) {
  // show_luma       Mark clamped pixels red/green over a colour image
  // show_luma_grey  Mark clamped pixels red/green over a greyscaled image
  float* srcp = reinterpret_cast<float*>(srcp8);
  float* srcpU = reinterpret_cast<float*>(srcpU8);
  float* srcpV = reinterpret_cast<float*>(srcpV8);
  pitch /= sizeof(float);
  pitchUV /= sizeof(float);

  for (int h = 0; h < height; h += 1) {
    for (int x = 0; x < width; x += 1) {
      if (srcp[x] < min_luma) {
        srcp[x] = c8tof(81);
        srcpU[x] = uv8tof(91);
        srcpV[x] = uv8tof(240);
      } // red:   Y=81, U=91 and V=240
      else if (srcp[x] > max_luma) {
        srcp[x] = c8tof(145);
        srcpU[x] = uv8tof(54);
        srcpV[x] = uv8tof(34);
      } // green: Y=145, U=54 and V=34
      // this differs from show_luma
      else if (show_luma_grey) {
        srcpU[x] = srcpV[x] = uv8tof(128);
      } // grey
    }
    srcp += pitch;
    srcpV += pitchUV;
    srcpU += pitchUV;
  }
}

template <bool show_luma_grey>
static void show_luma_with_grey_opt_yuv420_f(BYTE* srcp8, BYTE* srcpU8, BYTE* srcpV8, int pitch, int pitchUV, int width,
                                             int height, float min_luma, float max_luma) {
  // show_luma       Mark clamped pixels red/green over a colour image
  // show_luma_grey  Mark clamped pixels red/green over a greyscaled image
  float* srcp = reinterpret_cast<float*>(srcp8);
  float* srcn = reinterpret_cast<float*>(srcp8 + pitch); // next line
  float* srcpU = reinterpret_cast<float*>(srcpU8);
  float* srcpV = reinterpret_cast<float*>(srcpV8);
  pitch /= sizeof(float);
  pitchUV /= sizeof(float);

  for (int h = 0; h < height; h += 2) {
    for (int x = 0; x < width; x += 2) {
      int uv = 0;
      if (srcp[x] < min_luma) {
        srcp[x] = c8tof(81);
        uv |= 1;
      } else if (srcp[x] > max_luma) {
        srcp[x] = c8tof(145);
        uv |= 2;
      }
      if (srcp[x + 1] < min_luma) {
        srcp[x + 1] = c8tof(81);
        uv |= 1;
      } else if (srcp[x + 1] > max_luma) {
        srcp[x + 1] = c8tof(145);
        uv |= 2;
      }
      if (srcn[x] < min_luma) {
        srcn[x] = c8tof(81);
        uv |= 1;
      } else if (srcn[x] > max_luma) {
        srcn[x] = c8tof(145);
        uv |= 2;
      }
      if (srcn[x + 1] < min_luma) {
        srcn[x + 1] = c8tof(81);
        uv |= 1;
      } else if (srcn[x + 1] > max_luma) {
        srcn[x + 1] = c8tof(145);
        uv |= 2;
      }
      switch (uv) {
        case 1:
          srcpU[x / 2] = uv8tof(91);
          srcpV[x / 2] = uv8tof(240);
          break; // red:   Y=81, U=91 and V=240
        case 2:
          srcpU[x / 2] = uv8tof(54);
          srcpV[x / 2] = uv8tof(34);
          break; // green: Y=145, U=54 and V=34
                 // this differs from show_luma_grey
        case 3:
          if (show_luma_grey) {
            srcpU[x / 2] = uv8tof(90);
            srcpV[x / 2] = uv8tof(134);
            break; // puke:  Y=81, U=90 and V=134 olive: Y=145, U=90 and V=134
          } else {
            srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = c8tof(210); // yellow:Y=210, U=16 and V=146
            srcpU[x / 2] = uv8tof(16);
            srcpV[x / 2] = uv8tof(146);
          }
          break;
        default:
          if (show_luma_grey) {
            srcpU[x / 2] = srcpV[x / 2] = uv8tof(128); // olive: Y=145, U=90 and V=134
          }
          break;
      }
    }
    srcp += pitch * 2; // 2x2 pixels at a time (4:2:0 subsampling)
    srcn += pitch * 2;
    srcpV += pitchUV;
    srcpU += pitchUV;
  }
}

template <typename pixel_t>
static void show_chroma_yuv444(BYTE* srcp8, BYTE* srcpU8, BYTE* srcpV8, int pitch, int pitchUV, int width, int height,
                               int min_chroma, int max_chroma, int bits_per_pixel) {
  const int shift = sizeof(pixel_t) == 1 ? 0 : (bits_per_pixel - 8);
  pixel_t* srcp = reinterpret_cast<pixel_t*>(srcp8);
  pixel_t* srcpU = reinterpret_cast<pixel_t*>(srcpU8);
  pixel_t* srcpV = reinterpret_cast<pixel_t*>(srcpV8);
  pitch /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

  for (int h = 0; h < height; h += 1) {
    for (int x = 0; x < width; x += 1) {
      if ((srcpU[x] < min_chroma)     // U-
          || (srcpU[x] > max_chroma)  // U+
          || (srcpV[x] < min_chroma)  // V-
          || (srcpV[x] > max_chroma)) // V+
      {
        srcp[x] = 210 << shift;
        srcpU[x] = 16 << shift;
        srcpV[x] = 146 << shift;
      } // yellow:Y=210, U=16 and V=146
    }
    srcp += pitch;
    srcpV += pitchUV;
    srcpU += pitchUV;
  }
}

template <typename pixel_t>
static void show_chroma_yuv420(BYTE* srcp8, BYTE* srcpU8, BYTE* srcpV8, int pitch, int pitchUV, int width, int height,
                               int min_chroma, int max_chroma, int bits_per_pixel) {
  const int shift = sizeof(pixel_t) == 1 ? 0 : (bits_per_pixel - 8);
  pixel_t* srcp = reinterpret_cast<pixel_t*>(srcp8);
  pixel_t* srcn = reinterpret_cast<pixel_t*>(srcp8 + pitch); // next line
  pixel_t* srcpU = reinterpret_cast<pixel_t*>(srcpU8);
  pixel_t* srcpV = reinterpret_cast<pixel_t*>(srcpV8);
  pitch /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

  for (int h = 0; h < height; h += 2) {
    for (int x = 0; x < width; x += 2) {
      if ((srcpU[x / 2] < min_chroma)     // U-
          || (srcpU[x / 2] > max_chroma)  // U+
          || (srcpV[x / 2] < min_chroma)  // V-
          || (srcpV[x / 2] > max_chroma)) // V+
      {
        srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = 210 << shift;
        srcpU[x / 2] = 16 << shift;
        srcpV[x / 2] = 146 << shift;
      } // yellow:Y=210, U=16 and V=146
    }
    srcp += pitch * 2; // 2x2 pixels at a time (4:2:0 subsampling)
    srcn += pitch * 2;
    srcpV += pitchUV;
    srcpU += pitchUV;
  }
}

template <typename pixel_t>
static void show_chroma_grey_yuv444(BYTE* srcp8, BYTE* srcpU8, BYTE* srcpV8, int pitch, int pitchUV, int width,
                                    int height, int min_chroma, int max_chroma, int bits_per_pixel) {
  const int shift = sizeof(pixel_t) == 1 ? 0 : (bits_per_pixel - 8);
  pixel_t* srcp = reinterpret_cast<pixel_t*>(srcp8);
  pixel_t* srcpU = reinterpret_cast<pixel_t*>(srcpU8);
  pixel_t* srcpV = reinterpret_cast<pixel_t*>(srcpV8);
  pitch /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

  for (int h = 0; h < height; h += 1) {
    for (int x = 0; x < width; x += 1) {
      int uv = 0;
      if (srcpU[x] < min_chroma)
        uv |= 1; // U-
      else if (srcpU[x] > max_chroma)
        uv |= 2; // U+
      if (srcpV[x] < min_chroma)
        uv |= 4; // V-
      else if (srcpV[x] > max_chroma)
        uv |= 8; // V+
      switch (uv) {
        case 8:
          srcp[x] = 81 << shift;
          srcpU[x] = 91 << shift;
          srcpV[x] = 240 << shift;
          break; //   +V Red
        case 9:
          srcp[x] = 146 << shift;
          srcpU[x] = 53 << shift;
          srcpV[x] = 193 << shift;
          break; // -U+V Orange
        case 1:
          srcp[x] = 210 << shift;
          srcpU[x] = 16 << shift;
          srcpV[x] = 146 << shift;
          break; // -U   Yellow
        case 5:
          srcp[x] = 153 << shift;
          srcpU[x] = 49 << shift;
          srcpV[x] = 49 << shift;
          break; // -U-V Green
        case 4:
          srcp[x] = 170 << shift;
          srcpU[x] = 165 << shift;
          srcpV[x] = 16 << shift;
          break; //   -V Cyan
        case 6:
          srcp[x] = 105 << shift;
          srcpU[x] = 203 << shift;
          srcpV[x] = 63 << shift;
          break; // +U-V Teal
        case 2:
          srcp[x] = 41 << shift;
          srcpU[x] = 240 << shift;
          srcpV[x] = 110 << shift;
          break; // +U   Blue
        case 10:
          srcp[x] = 106 << shift;
          srcpU[x] = 202 << shift;
          srcpV[x] = 222 << shift;
          break; // +U+V Magenta
        default:
          srcpU[x] = srcpV[x] = 128 << shift;
          break;
      }
    }
    srcp += pitch;
    srcpV += pitchUV;
    srcpU += pitchUV;
  }
}

template <typename pixel_t>
static void show_chroma_grey_yuv420(BYTE* srcp8, BYTE* srcpU8, BYTE* srcpV8, int pitch, int pitchUV, int width,
                                    int height, int min_chroma, int max_chroma, int bits_per_pixel) {
  const int shift = sizeof(pixel_t) == 1 ? 0 : (bits_per_pixel - 8);
  pixel_t* srcp = reinterpret_cast<pixel_t*>(srcp8);
  pixel_t* srcn = reinterpret_cast<pixel_t*>(srcp8 + pitch); // next line
  pixel_t* srcpU = reinterpret_cast<pixel_t*>(srcpU8);
  pixel_t* srcpV = reinterpret_cast<pixel_t*>(srcpV8);
  pitch /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

  for (int h = 0; h < height; h += 2) {
    for (int x = 0; x < width; x += 2) {
      int uv = 0;
      if (srcpU[x / 2] < min_chroma)
        uv |= 1; // U-
      else if (srcpU[x / 2] > max_chroma)
        uv |= 2; // U+
      if (srcpV[x / 2] < min_chroma)
        uv |= 4; // V-
      else if (srcpV[x / 2] > max_chroma)
        uv |= 8; // V+
      switch (uv) {
        case 8:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = 81 << shift;
          srcpU[x / 2] = 91 << shift;
          srcpV[x / 2] = 240 << shift;
          break; //   +V Red
        case 9:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = 146 << shift;
          srcpU[x / 2] = 53 << shift;
          srcpV[x / 2] = 193 << shift;
          break; // -U+V Orange
        case 1:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = 210 << shift;
          srcpU[x / 2] = 16 << shift;
          srcpV[x / 2] = 146 << shift;
          break; // -U   Yellow
        case 5:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = 153 << shift;
          srcpU[x / 2] = 49 << shift;
          srcpV[x / 2] = 49 << shift;
          break; // -U-V Green
        case 4:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = 170 << shift;
          srcpU[x / 2] = 165 << shift;
          srcpV[x / 2] = 16 << shift;
          break; //   -V Cyan
        case 6:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = 105 << shift;
          srcpU[x / 2] = 203 << shift;
          srcpV[x / 2] = 63 << shift;
          break; // +U-V Teal
        case 2:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = 41 << shift;
          srcpU[x / 2] = 240 << shift;
          srcpV[x / 2] = 110 << shift;
          break; // +U   Blue
        case 10:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = 106 << shift;
          srcpU[x / 2] = 202 << shift;
          srcpV[x / 2] = 222 << shift;
          break; // +U+V Magenta
        default:
          srcpU[x / 2] = srcpV[x / 2] = 128 << shift;
          break;
      }
    }
    srcp += pitch * 2; // 2x2 pixels at a time (4:2:0 subsampling)
    srcn += pitch * 2;
    srcpV += pitchUV;
    srcpU += pitchUV;
  }
}

static void show_chroma_yuv444_f(BYTE* srcp8, BYTE* srcpU8, BYTE* srcpV8, int pitch, int pitchUV, int width, int height,
                                 float min_chroma, float max_chroma) {
  float* srcp = reinterpret_cast<float*>(srcp8);
  float* srcpU = reinterpret_cast<float*>(srcpU8);
  float* srcpV = reinterpret_cast<float*>(srcpV8);
  pitch /= sizeof(float);
  pitchUV /= sizeof(float);

  for (int h = 0; h < height; h += 1) {
    for (int x = 0; x < width; x += 1) {
      if ((srcpU[x] < min_chroma)     // U-
          || (srcpU[x] > max_chroma)  // U+
          || (srcpV[x] < min_chroma)  // V-
          || (srcpV[x] > max_chroma)) // V+
      {
        srcp[x] = c8tof(210);
        srcpU[x] = uv8tof(16);
        srcpV[x] = uv8tof(146);
      } // yellow:Y=210, U=16 and V=146
    }
    srcp += pitch;
    srcpV += pitchUV;
    srcpU += pitchUV;
  }
}

static void show_chroma_yuv420_f(BYTE* srcp8, BYTE* srcpU8, BYTE* srcpV8, int pitch, int pitchUV, int width, int height,
                                 float min_chroma, float max_chroma) {
  float* srcp = reinterpret_cast<float*>(srcp8);
  float* srcn = reinterpret_cast<float*>(srcp8 + pitch); // next line
  float* srcpU = reinterpret_cast<float*>(srcpU8);
  float* srcpV = reinterpret_cast<float*>(srcpV8);
  pitch /= sizeof(float);
  pitchUV /= sizeof(float);

  for (int h = 0; h < height; h += 2) {
    for (int x = 0; x < width; x += 2) {
      if ((srcpU[x / 2] < min_chroma)     // U-
          || (srcpU[x / 2] > max_chroma)  // U+
          || (srcpV[x / 2] < min_chroma)  // V-
          || (srcpV[x / 2] > max_chroma)) // V+
      {
        srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = c8tof(210);
        srcpU[x / 2] = uv8tof(16);
        srcpV[x / 2] = uv8tof(146);
      } // yellow:Y=210, U=16 and V=146
    }
    srcp += pitch * 2; // 2x2 pixels at a time (4:2:0 subsampling)
    srcn += pitch * 2;
    srcpV += pitchUV;
    srcpU += pitchUV;
  }
}

static void show_chroma_grey_yuv444_f(BYTE* srcp8, BYTE* srcpU8, BYTE* srcpV8, int pitch, int pitchUV, int width,
                                      int height, float min_chroma, float max_chroma) {
  float* srcp = reinterpret_cast<float*>(srcp8);
  float* srcpU = reinterpret_cast<float*>(srcpU8);
  float* srcpV = reinterpret_cast<float*>(srcpV8);
  pitch /= sizeof(float);
  pitchUV /= sizeof(float);

  for (int h = 0; h < height; h += 1) {
    for (int x = 0; x < width; x += 1) {
      int uv = 0;
      if (srcpU[x] < min_chroma)
        uv |= 1; // U-
      else if (srcpU[x] > max_chroma)
        uv |= 2; // U+
      if (srcpV[x] < min_chroma)
        uv |= 4; // V-
      else if (srcpV[x] > max_chroma)
        uv |= 8; // V+
      switch (uv) {
        case 8:
          srcp[x] = c8tof(81);
          srcpU[x] = uv8tof(91);
          srcpV[x] = uv8tof(240);
          break; //   +V Red
        case 9:
          srcp[x] = c8tof(146);
          srcpU[x] = uv8tof(53);
          srcpV[x] = uv8tof(193);
          break; // -U+V Orange
        case 1:
          srcp[x] = c8tof(210);
          srcpU[x] = uv8tof(16);
          srcpV[x] = uv8tof(146);
          break; // -U   Yellow
        case 5:
          srcp[x] = c8tof(153);
          srcpU[x] = uv8tof(49);
          srcpV[x] = uv8tof(49);
          break; // -U-V Green
        case 4:
          srcp[x] = c8tof(170);
          srcpU[x] = uv8tof(165);
          srcpV[x] = uv8tof(16);
          break; //   -V Cyan
        case 6:
          srcp[x] = c8tof(105);
          srcpU[x] = uv8tof(203);
          srcpV[x] = uv8tof(63);
          break; // +U-V Teal
        case 2:
          srcp[x] = c8tof(41);
          srcpU[x] = uv8tof(240);
          srcpV[x] = uv8tof(110);
          break; // +U   Blue
        case 10:
          srcp[x] = c8tof(106);
          srcpU[x] = uv8tof(202);
          srcpV[x] = uv8tof(222);
          break; // +U+V Magenta
        default:
          srcpU[x] = srcpV[x] = uv8tof(128);
          break;
      }
    }
    srcp += pitch;
    srcpV += pitchUV;
    srcpU += pitchUV;
  }
}

static void show_chroma_grey_yuv420_f(BYTE* srcp8, BYTE* srcpU8, BYTE* srcpV8, int pitch, int pitchUV, int width,
                                      int height, float min_chroma, float max_chroma) {
  float* srcp = reinterpret_cast<float*>(srcp8);
  float* srcn = reinterpret_cast<float*>(srcp8 + pitch); // next line
  float* srcpU = reinterpret_cast<float*>(srcpU8);
  float* srcpV = reinterpret_cast<float*>(srcpV8);
  pitch /= sizeof(float);
  pitchUV /= sizeof(float);

  for (int h = 0; h < height; h += 2) {
    for (int x = 0; x < width; x += 2) {
      int uv = 0;
      if (srcpU[x / 2] < min_chroma)
        uv |= 1; // U-
      else if (srcpU[x / 2] > max_chroma)
        uv |= 2; // U+
      if (srcpV[x / 2] < min_chroma)
        uv |= 4; // V-
      else if (srcpV[x / 2] > max_chroma)
        uv |= 8; // V+
      switch (uv) {
        case 8:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = c8tof(81);
          srcpU[x / 2] = uv8tof(91);
          srcpV[x / 2] = uv8tof(240);
          break; //   +V Red
        case 9:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = c8tof(146);
          srcpU[x / 2] = uv8tof(53);
          srcpV[x / 2] = uv8tof(193);
          break; // -U+V Orange
        case 1:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = c8tof(210);
          srcpU[x / 2] = uv8tof(16);
          srcpV[x / 2] = uv8tof(146);
          break; // -U   Yellow
        case 5:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = c8tof(153);
          srcpU[x / 2] = uv8tof(49);
          srcpV[x / 2] = uv8tof(49);
          break; // -U-V Green
        case 4:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = c8tof(170);
          srcpU[x / 2] = uv8tof(165);
          srcpV[x / 2] = uv8tof(16);
          break; //   -V Cyan
        case 6:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = c8tof(105);
          srcpU[x / 2] = uv8tof(203);
          srcpV[x / 2] = uv8tof(63);
          break; // +U-V Teal
        case 2:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = c8tof(41);
          srcpU[x / 2] = uv8tof(240);
          srcpV[x / 2] = uv8tof(110);
          break; // +U   Blue
        case 10:
          srcp[x] = srcp[x + 1] = srcn[x] = srcn[x + 1] = c8tof(106);
          srcpU[x / 2] = uv8tof(202);
          srcpV[x / 2] = uv8tof(222);
          break; // +U+V Magenta
        default:
          srcpU[x / 2] = srcpV[x / 2] = uv8tof(128);
          break;
      }
    }
    srcp += pitch * 2; // 2x2 pixels at a time (4:2:0 subsampling)
    srcn += pitch * 2;
    srcpV += pitchUV;
    srcpU += pitchUV;
  }
}

} // namespace
extern "C" int aif_limiter_show(uint8_t* const data[3], const int pitches[3], int width, int height,
                                const aif_limiter_limits* l, int layout, int show) {
  if (!aif::limiter::valid(l) || !data || !pitches || width <= 0 || height <= 0 || layout < 0 || layout > 2 ||
      show < 1 || show > 4)
    return 1;
  int bits_per_pixel = l->bits, pixelsize = l->bits == 8 ? 1 : l->bits == 32 ? 4 : 2;
  if ((layout == 1 && (width % 2 || height % 2)) || (layout == 2 && (width % 2 || pixelsize != 1)))
    return 1;
  if (width > std::numeric_limits<int>::max() / pixelsize / 2)
    return 1;
  for (int c = 0; c < (layout == 2 ? 1 : 3); ++c) {
    int rw = width * pixelsize * (layout == 2 ? 2 : 1) / (layout == 1 && c ? 2 : 1);
    if (!data[c] || pitches[c] < rw || pitches[c] % pixelsize || reinterpret_cast<uintptr_t>(data[c]) % pixelsize)
      return 1;
  }
  if (layout != 2 && pitches[1] != pitches[2])
    return 1;
  int pitch = pitches[0], pitchUV = pitches[1], row_size = width * 2;
  auto* srcp = data[0];
  auto* srcpU = data[1];
  auto* srcpV = data[2];
  int min_luma = pixelsize == 4 ? 0 : int(l->min_luma), max_luma = pixelsize == 4 ? 0 : int(l->max_luma),
      min_chroma = pixelsize == 4 ? 0 : int(l->min_chroma), max_chroma = pixelsize == 4 ? 0 : int(l->max_chroma);
  float min_luma_f = l->min_luma, max_luma_f = l->max_luma, min_chroma_f = l->min_chroma, max_chroma_f = l->max_chroma;
  if (layout == 2) {
    if (show == show_luma) { // Mark clamped pixels red/yellow/green over a colour image
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < row_size; x += 4) {
          int uv = 0;
          if (srcp[x] < min_luma) {
            srcp[x] = 81;
            uv |= 1;
          } else if (srcp[x] > max_luma) {
            srcp[x] = 145;
            uv |= 2;
          }
          if (srcp[x + 2] < min_luma) {
            srcp[x + 2] = 81;
            uv |= 1;
          } else if (srcp[x + 2] > max_luma) {
            srcp[x + 2] = 145;
            uv |= 2;
          }
          switch (uv) {
            case 1:
              srcp[x + 1] = 91;
              srcp[x + 3] = 240;
              break; // red:   Y= 81, U=91 and V=240
            case 2:
              srcp[x + 1] = 54;
              srcp[x + 3] = 34;
              break; // green: Y=145, U=54 and V=34
            case 3:
              srcp[x] = srcp[x + 2] = 210;
              srcp[x + 1] = 16;
              srcp[x + 3] = 146;
              break; // yellow:Y=210, U=16 and V=146
            default:
              break;
          }
        }
        srcp += pitch;
      }
      return 0;
    } else if (show == show_luma_grey) { // Mark clamped pixels coloured over a greyscaled image
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < row_size; x += 4) {
          int uv = 0;
          if (srcp[x] < min_luma) {
            srcp[x] = 81;
            uv |= 1;
          } else if (srcp[x] > max_luma) {
            srcp[x] = 145;
            uv |= 2;
          }
          if (srcp[x + 2] < min_luma) {
            srcp[x + 2] = 81;
            uv |= 1;
          } else if (srcp[x + 2] > max_luma) {
            srcp[x + 2] = 145;
            uv |= 2;
          }
          switch (uv) {
            case 1:
              srcp[x + 1] = 91;
              srcp[x + 3] = 240;
              break; // red:   Y=81, U=91 and V=240
            case 2:
              srcp[x + 1] = 54;
              srcp[x + 3] = 34;
              break; // green: Y=145, U=54 and V=34
            case 3:
              srcp[x + 1] = 90;
              srcp[x + 3] = 134;
              break; // puke:  Y=81, U=90 and V=134
            default:
              srcp[x + 1] = srcp[x + 3] = 128;
              break; // olive: Y=145, U=90 and V=134
          }
        }
        srcp += pitch;
      }
      return 0;
    } else if (show == show_chroma) { // Mark clamped pixels yellow over a colour image
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < row_size; x += 4) {
          if ((srcp[x + 1] < min_chroma)     // U-
              || (srcp[x + 1] > max_chroma)  // U+
              || (srcp[x + 3] < min_chroma)  // V-
              || (srcp[x + 3] > max_chroma)) // V+
          {
            srcp[x] = srcp[x + 2] = 210;
            srcp[x + 1] = 16;
            srcp[x + 3] = 146;
          } // yellow:Y=210, U=16 and V=146
        }
        srcp += pitch;
      }
      return 0;
    } else if (show == show_chroma_grey) { // Mark clamped pixels coloured over a greyscaled image
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < row_size; x += 4) {
          int uv = 0;
          if (srcp[x + 1] < min_chroma)
            uv |= 1; // U-
          else if (srcp[x + 1] > max_chroma)
            uv |= 2; // U+
          if (srcp[x + 3] < min_chroma)
            uv |= 4; // V-
          else if (srcp[x + 3] > max_chroma)
            uv |= 8; // V+
          switch (uv) {
            case 8:
              srcp[x] = srcp[x + 2] = 81;
              srcp[x + 1] = 91;
              srcp[x + 3] = 240;
              break; //   +V Red
            case 9:
              srcp[x] = srcp[x + 2] = 146;
              srcp[x + 1] = 53;
              srcp[x + 3] = 193;
              break; // -U+V Orange
            case 1:
              srcp[x] = srcp[x + 2] = 210;
              srcp[x + 1] = 16;
              srcp[x + 3] = 146;
              break; // -U   Yellow
            case 5:
              srcp[x] = srcp[x + 2] = 153;
              srcp[x + 1] = 49;
              srcp[x + 3] = 49;
              break; // -U-V Green
            case 4:
              srcp[x] = srcp[x + 2] = 170;
              srcp[x + 1] = 165;
              srcp[x + 3] = 16;
              break; //   -V Cyan
            case 6:
              srcp[x] = srcp[x + 2] = 105;
              srcp[x + 1] = 203;
              srcp[x + 3] = 63;
              break; // +U-V Teal
            case 2:
              srcp[x] = srcp[x + 2] = 41;
              srcp[x + 1] = 240;
              srcp[x + 3] = 110;
              break; // +U   Blue
            case 10:
              srcp[x] = srcp[x + 2] = 106;
              srcp[x + 1] = 202;
              srcp[x + 3] = 222;
              break; // +U+V Magenta
            default:
              srcp[x + 1] = srcp[x + 3] = 128;
              break;
          }
        }
        srcp += pitch;
      }
      return 0;
    }
  } else if (layout == 1) {

    if (show == show_luma || show == show_luma_grey) { // Mark clamped pixels red/yellow/green over a colour image
      if (pixelsize == 1) {
        if (show == show_luma)
          show_luma_with_grey_opt_yuv420<uint8_t, false>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_luma,
                                                         max_luma, bits_per_pixel);
        else // show_luma_grey
          show_luma_with_grey_opt_yuv420<uint8_t, true>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_luma,
                                                        max_luma, bits_per_pixel);
      } else if (pixelsize == 2) { // pixelsize == 2
        if (show == show_luma)
          show_luma_with_grey_opt_yuv420<uint16_t, false>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_luma,
                                                          max_luma, bits_per_pixel);
        else // show_luma_grey
          show_luma_with_grey_opt_yuv420<uint16_t, true>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_luma,
                                                         max_luma, bits_per_pixel);
      } else { // pixelsize == 4
        if (show == show_luma)
          show_luma_with_grey_opt_yuv420_f<false>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_luma_f,
                                                  max_luma_f);
        else // show_luma_grey
          show_luma_with_grey_opt_yuv420_f<true>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_luma_f,
                                                 max_luma_f);
      }
      return 0;
    } else if (show == show_chroma) { // Mark clamped pixels yellow over a colour image
      if (pixelsize == 1)
        show_chroma_yuv420<uint8_t>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_chroma, max_chroma,
                                    bits_per_pixel);
      else if (pixelsize == 2)
        show_chroma_yuv420<uint16_t>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_chroma, max_chroma,
                                     bits_per_pixel);
      else // float
        show_chroma_yuv420_f(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_chroma_f, max_chroma_f);
      return 0;
    } else if (show == show_chroma_grey) { // Mark clamped pixels coloured over a greyscaled image
      if (pixelsize == 1)
        show_chroma_grey_yuv420<uint8_t>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_chroma, max_chroma,
                                         bits_per_pixel);
      else if (pixelsize == 2)
        show_chroma_grey_yuv420<uint16_t>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_chroma, max_chroma,
                                          bits_per_pixel);
      else // float
        show_chroma_grey_yuv420_f(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_chroma_f, max_chroma_f);
      return 0;
    }
    // YV12 (4:2:0) end
  } else if (layout == 0) {

    if (show == show_luma || show == show_luma_grey) {
      if (pixelsize == 1) {
        if (show == show_luma)
          show_luma_with_grey_opt_yuv444<uint8_t, false>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_luma,
                                                         max_luma, bits_per_pixel);
        else // show_luma_grey
          show_luma_with_grey_opt_yuv444<uint8_t, true>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_luma,
                                                        max_luma, bits_per_pixel);
      } else if (pixelsize == 2) {
        if (show == show_luma)
          show_luma_with_grey_opt_yuv444<uint16_t, false>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_luma,
                                                          max_luma, bits_per_pixel);
        else // show_luma_grey
          show_luma_with_grey_opt_yuv444<uint16_t, true>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_luma,
                                                         max_luma, bits_per_pixel);
      } else { // pixelsize == 4
        if (show == show_luma)
          show_luma_with_grey_opt_yuv444_f<false>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_luma_f,
                                                  max_luma_f);
        else // show_luma_grey
          show_luma_with_grey_opt_yuv444_f<true>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_luma_f,
                                                 max_luma_f);
      }
      return 0;
    } else if (show == show_chroma) { // Mark clamped pixels yellow over a colour image
      if (pixelsize == 1)
        show_chroma_yuv444<uint8_t>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_chroma, max_chroma,
                                    bits_per_pixel);
      else if (pixelsize == 2)
        show_chroma_yuv444<uint16_t>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_chroma, max_chroma,
                                     bits_per_pixel);
      else // float
        show_chroma_yuv444_f(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_chroma_f, max_chroma_f);
      return 0;
    } else if (show == show_chroma_grey) { // Mark clamped pixels coloured over a greyscaled image
      if (pixelsize == 1)
        show_chroma_grey_yuv444<uint8_t>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_chroma, max_chroma,
                                         bits_per_pixel);
      else if (pixelsize == 2)
        show_chroma_grey_yuv444<uint16_t>(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_chroma, max_chroma,
                                          bits_per_pixel);
      else // float
        show_chroma_grey_yuv444_f(srcp, srcpU, srcpV, pitch, pitchUV, width, height, min_chroma_f, max_chroma_f);
      return 0;
    }
    // YV24 (4:4:4) end
  }
  return 1;
}
