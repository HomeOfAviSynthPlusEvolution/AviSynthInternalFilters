// Derived from AviSynth convert_helper.h, GPL-2.0-or-later with linking exception.
#pragma once
namespace aif::filters::histogram {
typedef struct bits_conv_constants {
  float src_offset = 0.0f;
  int src_offset_i = 0;
  float mul_factor = 1.0f;
  float dst_offset = 0.0f;
} bits_conv_constants;

// universal transform values for any full-limited bit-depth mix
[[maybe_unused]] inline void get_bits_conv_constants(bits_conv_constants& d, bool use_chroma, bool fulls, bool fulld,
                                                     int srcBitDepth, int dstBitDepth) {
  d.src_offset = 0.0f;
  d.mul_factor = 1.0f;
  d.dst_offset = 0.0f;
  float src_span = 1.0f;
  float dst_span = 1.0f;

  // possible usage places
  // Expr (autoscale, scalef, scaleb)
  // convert_bits
  // ColorYUV
  // Histogram

  if (srcBitDepth != dstBitDepth || fulls != fulld) {
    // fulls != fulld
    if (use_chroma) {
      // chroma: go into signed world, factor, then go back to biased range
      d.src_offset = (srcBitDepth == 32) ? 0.0f : (1 << (srcBitDepth - 1));
      d.dst_offset = (dstBitDepth == 32) ? 0.0f : (1 << (dstBitDepth - 1));
      // decision: 'limited' range float +/-112 is +/-112/255.0. Must be consistent with Expr, ColorYUV etc
      // 3.7.2: full range chroma: as per ITU Rec H.273 eq.30 p.10: Cb=Clip1_C(Round(((1<<BitDepth_C)-1)*E'_PB)+(1<<(BitDepth_C-1)))
      // For 8 bit this results in 128 +/-127.5 instead of 128 +/-127
      // In general the span is ((1 << srcBitDepth) - 1) / 2.0 instead of (1 << (srcBitDepth - 1)) - 1
      src_span = (srcBitDepth == 32)
                     ? (fulls ? 0.5f : 112 / 255.0f)
                     : (fulls ? (float)((1 << srcBitDepth) - 1) / 2.0f : (float)(112 << (srcBitDepth - 8)));
      dst_span = (dstBitDepth == 32)
                     ? (fulld ? 0.5f : 112 / 255.0f)
                     : (fulld ? (float)((1 << dstBitDepth) - 1) / 2.0f : (float)(112 << (dstBitDepth - 8)));

      /*
      fulls=fulld=false case:
      // mul/div by 2^N when between 8-16 bit integer formats
      // 255*: for consistent float conversion. scale (shift) to/from 8 bit. int-float is always 0..1.0 <-> 0..255
      // int-float: 10+ bits first downshift to 8 bits then /255.
      // float-int: 10+ bits first *255 to have 0..255 range, then shift from 8 bits to actual bit depth
      // Since first we convert to 8 bit 0-255 range and do bit-shift after then to >8 depths, the 1.0 number will show up as 255*256 in 16 bits.
      // This is normal. Like float32.ConvertBits(8).ConvertBits(16)
      (srcBitDepth == 32) -> (255 * (float)(1 << (dstBitDepth - 8)) / 1.0
      (dstBitDepth == 32) -> 1.0 / (255 * (float)(1 << (srcBitDepth - 8)))

      (srcBitDepth == 32) -> ((float)(1 << (dstBitDepth - 8)))) / (1 / 255.0f)
      (dstBitDepth == 32) -> 1.0 / 255.0f / (float)(1 << (srcBitDepth - 8))))
    }
    */

    } else {
      // luma
      d.src_offset = (srcBitDepth == 32) ? (fulls ? 0 : 16.0f / 255) : (fulls ? 0 : (16 << (srcBitDepth - 8)));
      d.dst_offset = (dstBitDepth == 32) ? (fulld ? 0 : 16.0f / 255) : (fulld ? 0 : (16 << (dstBitDepth - 8)));

      // false-false
      //src_span = (srcBitDepth == 32) ? 1.0f : (255 * (float)(1 << (srcBitDepth - 8)));
      //dst_span = (dstBitDepth == 32) ? 1.0f : (255 * (float)(1 << (dstBitDepth - 8)));

      src_span = (srcBitDepth == 32)
                     ? (fulls ? 1.0f : 219 / 255.0f)
                     : (fulls ? ((1 << srcBitDepth) - 1) : (219 << (srcBitDepth - 8))); // 0..255, 16..235
      dst_span = (dstBitDepth == 32) ? (fulld ? 1.0f : 219 / 255.0f)
                                     : (fulld ? ((1 << dstBitDepth) - 1) : (219 << (dstBitDepth - 8)));
    }
  }

  d.mul_factor = dst_span / src_span;
  d.src_offset_i = (int)d.src_offset; // no rounding, when used, it is integer
}

} // namespace aif::filters::histogram
