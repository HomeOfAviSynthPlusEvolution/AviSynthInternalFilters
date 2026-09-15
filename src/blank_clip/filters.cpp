// SPDX-License-Identifier: GPL-2.0-or-later
#include "blank_clip/filters.h"
#include "blank_clip.h"
namespace aif::filters::blank_clip {
const std::array<Registration, 6>& registrations() {
  static const std::array<Registration, 6> functions = {{
      {"BlankClip",
       "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i"
       "[audio_rate]i[stereo]b[sixteen_bit]b[color]i[color_yuv]i[clip]c",
       create_blank_clip, nullptr},
      {"BlankClip",
       "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i"
       "[audio_rate]i[channels]i[sample_type]s[color]i[color_yuv]i[clip]c",
       create_blank_clip, nullptr},
      {"BlankClip",
       "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i"
       "[audio_rate]i[stereo]b[sixteen_bit]b[color]i[color_yuv]i[clip]c[colors]f+",
       create_blank_clip, nullptr},
      {"BlankClip",
       "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i"
       "[audio_rate]i[channels]i[sample_type]s[color]i[color_yuv]i[clip]c[colors]f+",
       create_blank_clip, nullptr},
      {"Blackness",
       "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i"
       "[audio_rate]i[stereo]b[sixteen_bit]b[color]i[color_yuv]i[clip]c",
       create_blank_clip, nullptr},
      {"Blackness",
       "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i"
       "[audio_rate]i[channels]i[sample_type]s[color]i[color_yuv]i[clip]c",
       create_blank_clip, nullptr},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::blank_clip
