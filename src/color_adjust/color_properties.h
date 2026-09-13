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
#include <cctype>
namespace {

struct ColorRange_e {
  static constexpr int AVS_RANGE_FULL = 0;
  static constexpr int AVS_RANGE_LIMITED = 1;
};

struct ChromaLocation_e {
  static constexpr int AVS_CHROMA_CENTER = 1;
};

struct Matrix_e {
  static constexpr int AVS_MATRIX_UNSPECIFIED = 2;
  static constexpr int AVS_MATRIX_BT470_BG = 5;
};

[[nodiscard]] int coloryuv_stricmp(const char* left, const char* right) noexcept {
  if (left == nullptr || right == nullptr) {
    return left == right ? 0 : (left == nullptr ? -1 : 1);
  }
  while (*left != '\0' && *right != '\0') {
    const auto left_char = static_cast<unsigned char>(*left);
    const auto right_char = static_cast<unsigned char>(*right);
    const int difference = std::tolower(left_char) - std::tolower(right_char);
    if (difference != 0) {
      return difference;
    }
    ++left;
    ++right;
  }
  return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

void set_color_yuv_property(AVSMap* const properties, const char* const name, const int value,
                            IScriptEnvironment* const env) {
  if (value >= 0) {
    static_cast<void>(env->propSetInt(properties, name, value, PROPAPPENDMODE_REPLACE));
  } else {
    static_cast<void>(env->propDeleteKey(properties, name));
  }
}

void update_color_yuv_matrix_and_range(AVSMap* const properties, const int matrix, const int color_range,
                                       IScriptEnvironment* const env) {
  if (color_range == ColorRange_e::AVS_RANGE_FULL || color_range == ColorRange_e::AVS_RANGE_LIMITED) {
    static_cast<void>(env->propSetInt(properties, "_ColorRange", color_range, PROPAPPENDMODE_REPLACE));
  } else {
    static_cast<void>(env->propDeleteKey(properties, "_ColorRange"));
  }
  set_color_yuv_property(properties, "_Matrix", matrix, env);
}

void update_color_yuv_chroma_location(AVSMap* const properties, const int chroma_location,
                                      IScriptEnvironment* const env) {
  set_color_yuv_property(properties, "_ChromaLocation", chroma_location, env);
}

void update_color_yuv_range(AVSMap* const properties, const int color_range, IScriptEnvironment* const env) {
  if (color_range == ColorRange_e::AVS_RANGE_FULL || color_range == ColorRange_e::AVS_RANGE_LIMITED) {
    static_cast<void>(env->propSetInt(properties, "_ColorRange", color_range, PROPAPPENDMODE_REPLACE));
  } else {
    static_cast<void>(env->propDeleteKey(properties, "_ColorRange"));
  }
}

void read_color_yuv_properties(const AVSMap* const properties, int& matrix, int& color_range, int& output_color_range,
                               const int /*default_color_range*/, IScriptEnvironment* const env) {
  matrix = Matrix_e::AVS_MATRIX_UNSPECIFIED;
  // Current core's null matrix-name path takes the input property's range,
  // defaulting to limited for YUV, regardless of its suggested range argument.
  color_range = ColorRange_e::AVS_RANGE_LIMITED;
  output_color_range = ColorRange_e::AVS_RANGE_LIMITED;
  if (properties == nullptr) {
    return;
  }
  if (env->propNumElements(properties, "_Matrix") > 0) {
    matrix = static_cast<int>(env->propGetIntSaturated(properties, "_Matrix", 0, nullptr));
  }
  if (env->propNumElements(properties, "_ColorRange") > 0) {
    color_range = static_cast<int>(env->propGetIntSaturated(properties, "_ColorRange", 0, nullptr));
  }
}

} // namespace
