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
template <bool chroma, bool use_gamma>
AVS_FORCEINLINE float Levels::calcPixel(const float pixel) {
  float result;
  if (!chroma) {
    float p;
    if (coring)
      p = ((pixel - limits.tv_range_low_luma_f) * (1.0f / limits.range_luma_f) - in_min_f) / divisor_f;
    else
      p = (pixel - in_min_f) / divisor_f;

    if (use_gamma)
      p = (float)pow((double)clamp(p, 0.0f, 1.0f), gamma);

    p = p * out_diff_f + out_min_f; // out_diff_f = out_max_f - out_min_f;
    // luma
    if (coring) {
      result = clamp(p * limits.range_luma_f / 1.0f + limits.tv_range_low_luma_f, limits.tv_range_low_luma_f,
                     limits.tv_range_hi_luma_f);
    } else
      result = clamp(p, 0.0f, 1.0f); // todo: theoretical question, should we clamp in Levels function?
  } else {
    /*
      int q = (int)(((bias_dither + ii - middle_chroma * scale) * (out_max - out_min)) / divisor + middle_chroma + 0.5);
      int chroma;
      if (coring)
        chroma = clamp(q, tv_range_low, tv_range_hi_chroma); // e.g. clamp(q, 16, 240)
      else
        chroma = clamp(q, 0, max_pixel_value); // e.g. clamp(q, 0, 255)
      */
    float q = ((pixel - limits.middle_chroma_f) * out_diff_f) / divisor_f + limits.middle_chroma_f;
    if (coring)
      result = clamp(q, limits.tv_range_low_chroma_f, limits.tv_range_hi_chroma_f); // e.g. clamp(q, 16, 240)
    else
      result = clamp(
          q, limits.full_range_low_chroma_f,
          limits
              .full_range_hi_chroma_f); // e.g. clamp(q, 0, 255) todo: theoretical question, should we clamp in Levels function?
  }
  return result;
}
