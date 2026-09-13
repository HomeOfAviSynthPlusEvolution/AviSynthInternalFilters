// SPDX-License-Identifier: GPL-2.0-or-later
#include "blank_clip/filters.h"
#include "blank_clip.h"
namespace aif::filters::blank_clip {
void register_blank_clip(IScriptEnvironment* const env, const char* const function_name) {
  env->AddFunction(function_name,
                   "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i"
                   "[audio_rate]i[stereo]b[sixteen_bit]b[color]i[color_yuv]i[clip]c",
                   create_blank_clip, nullptr);
  env->AddFunction(function_name,
                   "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i"
                   "[audio_rate]i[channels]i[sample_type]s[color]i[color_yuv]i[clip]c",
                   create_blank_clip, nullptr);
  env->AddFunction(function_name,
                   "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i"
                   "[audio_rate]i[stereo]b[sixteen_bit]b[color]i[color_yuv]i[clip]c[colors]f+",
                   create_blank_clip, nullptr);
  env->AddFunction(function_name,
                   "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i"
                   "[audio_rate]i[channels]i[sample_type]s[color]i[color_yuv]i[clip]c[colors]f+",
                   create_blank_clip, nullptr);
}

void register_blackness(IScriptEnvironment* const env, const char* const function_name) {
  env->AddFunction(function_name,
                   "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i"
                   "[audio_rate]i[stereo]b[sixteen_bit]b[color]i[color_yuv]i[clip]c",
                   create_blank_clip, nullptr);
  env->AddFunction(function_name,
                   "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i"
                   "[audio_rate]i[channels]i[sample_type]s[color]i[color_yuv]i[clip]c",
                   create_blank_clip, nullptr);
}

void register_filters(IScriptEnvironment* env) {
  register_blank_clip(env, "IFBlankClip");
  register_blackness(env, "IFBlackness");
}
} // namespace aif::filters::blank_clip
