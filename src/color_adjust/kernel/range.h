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
struct bits_conv_constants {
  float src_offset = 0.0F;
  int src_offset_i = 0;
  float mul_factor = 1.0F;
  float dst_offset = 0.0F;
  float src_span = 1.0F;
  float dst_span = 1.0F;
};

// This is the small range-conversion helper used by ColorYUV's upstream LUT. Keeping it local
// avoids pulling the rest of AviSynthPlus' conversion subsystem into the plugin target.
void get_bits_conv_constants(bits_conv_constants& d, const bool use_chroma, const bool fulls, const bool fulld,
                             const int src_bit_depth, const int dst_bit_depth) {
  d.src_offset = 0.0F;
  d.mul_factor = 1.0F;
  d.dst_offset = 0.0F;
  d.src_span = 1.0F;
  d.dst_span = 1.0F;

  if (use_chroma) {
    d.src_span = src_bit_depth == 32 ? (fulls ? 0.5F : 112.0F / 255.0F)
                                     : (fulls ? static_cast<float>((1 << src_bit_depth) - 1) / 2.0F
                                              : static_cast<float>(112 << (src_bit_depth - 8)));
    d.dst_span = dst_bit_depth == 32 ? (fulld ? 0.5F : 112.0F / 255.0F)
                                     : (fulld ? static_cast<float>((1 << dst_bit_depth) - 1) / 2.0F
                                              : static_cast<float>(112 << (dst_bit_depth - 8)));
    d.src_offset = src_bit_depth == 32 ? 0.0F : static_cast<float>(1 << (src_bit_depth - 1));
    d.dst_offset = dst_bit_depth == 32 ? 0.0F : static_cast<float>(1 << (dst_bit_depth - 1));
  } else {
    d.src_span = src_bit_depth == 32 ? (fulls ? 1.0F : 219.0F / 255.0F)
                                     : (fulls ? static_cast<float>((1 << src_bit_depth) - 1)
                                              : static_cast<float>(219 << (src_bit_depth - 8)));
    d.dst_span = dst_bit_depth == 32 ? (fulld ? 1.0F : 219.0F / 255.0F)
                                     : (fulld ? static_cast<float>((1 << dst_bit_depth) - 1)
                                              : static_cast<float>(219 << (dst_bit_depth - 8)));
    d.src_offset = src_bit_depth == 32 ? (fulls ? 0.0F : 16.0F / 255.0F)
                                       : (fulls ? 0.0F : static_cast<float>(16 << (src_bit_depth - 8)));
    d.dst_offset = dst_bit_depth == 32 ? (fulld ? 0.0F : 16.0F / 255.0F)
                                       : (fulld ? 0.0F : static_cast<float>(16 << (dst_bit_depth - 8)));
  }

  d.mul_factor = d.dst_span / d.src_span;
  d.src_offset_i = static_cast<int>(d.src_offset);
}
