// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2002 Ben Rudiak-Gould et al.
// Copyright (C) 2026 AviSynthPlus-IF contributors
// Derived from AviSynthPlus avs_core/filters/source.cpp.

#include "blank_clip.h"
#include "static_image.h"
#include "single_frame.h"
#include "kernel_adapter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace aif::filters::blank_clip {
namespace {

constexpr int kMatrixRgb = 0;
constexpr int kMatrixSt170M = 6;
constexpr int kColorRangeFull = 0;
constexpr int kColorRangeLimited = 1;

enum class ColorMode {
  rgb,
  yuv,
};

struct ColorValues {
  int packed_color{};
  ColorMode mode{ColorMode::rgb};
  std::array<int, 4> integer_components{};
  std::array<float, 4> float_components{};
  bool is_array{};
};

struct PixelTypeName {
  int pixel_type;
  std::string_view name;
};

constexpr std::array kPixelTypeNames{
    PixelTypeName{VideoInfo::CS_BGR24, "RGB24"},
    PixelTypeName{VideoInfo::CS_BGR32, "RGB32"},
    PixelTypeName{VideoInfo::CS_YUY2, "YUY2"},
    PixelTypeName{VideoInfo::CS_YV24, "YV24"},
    PixelTypeName{VideoInfo::CS_YV16, "YV16"},
    PixelTypeName{VideoInfo::CS_YV12, "YV12"},
    PixelTypeName{VideoInfo::CS_I420, "YV12"},
    PixelTypeName{VideoInfo::CS_YUV9, "YUV9"},
    PixelTypeName{VideoInfo::CS_YV411, "YV411"},
    PixelTypeName{VideoInfo::CS_Y8, "Y8"},
    PixelTypeName{VideoInfo::CS_YUV420P10, "YUV420P10"},
    PixelTypeName{VideoInfo::CS_YUV422P10, "YUV422P10"},
    PixelTypeName{VideoInfo::CS_YUV444P10, "YUV444P10"},
    PixelTypeName{VideoInfo::CS_Y10, "Y10"},
    PixelTypeName{VideoInfo::CS_YUV420P12, "YUV420P12"},
    PixelTypeName{VideoInfo::CS_YUV422P12, "YUV422P12"},
    PixelTypeName{VideoInfo::CS_YUV444P12, "YUV444P12"},
    PixelTypeName{VideoInfo::CS_Y12, "Y12"},
    PixelTypeName{VideoInfo::CS_YUV420P14, "YUV420P14"},
    PixelTypeName{VideoInfo::CS_YUV422P14, "YUV422P14"},
    PixelTypeName{VideoInfo::CS_YUV444P14, "YUV444P14"},
    PixelTypeName{VideoInfo::CS_Y14, "Y14"},
    PixelTypeName{VideoInfo::CS_YUV420P16, "YUV420P16"},
    PixelTypeName{VideoInfo::CS_YUV422P16, "YUV422P16"},
    PixelTypeName{VideoInfo::CS_YUV444P16, "YUV444P16"},
    PixelTypeName{VideoInfo::CS_Y16, "Y16"},
    PixelTypeName{VideoInfo::CS_YUV420PS, "YUV420PS"},
    PixelTypeName{VideoInfo::CS_YUV422PS, "YUV422PS"},
    PixelTypeName{VideoInfo::CS_YUV444PS, "YUV444PS"},
    PixelTypeName{VideoInfo::CS_Y32, "Y32"},
    PixelTypeName{VideoInfo::CS_BGR48, "RGB48"},
    PixelTypeName{VideoInfo::CS_BGR64, "RGB64"},
    PixelTypeName{VideoInfo::CS_RGBP, "RGBP"},
    PixelTypeName{VideoInfo::CS_RGBP10, "RGBP10"},
    PixelTypeName{VideoInfo::CS_RGBP12, "RGBP12"},
    PixelTypeName{VideoInfo::CS_RGBP14, "RGBP14"},
    PixelTypeName{VideoInfo::CS_RGBP16, "RGBP16"},
    PixelTypeName{VideoInfo::CS_RGBPS, "RGBPS"},
    PixelTypeName{VideoInfo::CS_YUVA420, "YUVA420"},
    PixelTypeName{VideoInfo::CS_YUVA422, "YUVA422"},
    PixelTypeName{VideoInfo::CS_YUVA444, "YUVA444"},
    PixelTypeName{VideoInfo::CS_YUVA420P10, "YUVA420P10"},
    PixelTypeName{VideoInfo::CS_YUVA422P10, "YUVA422P10"},
    PixelTypeName{VideoInfo::CS_YUVA444P10, "YUVA444P10"},
    PixelTypeName{VideoInfo::CS_YUVA420P12, "YUVA420P12"},
    PixelTypeName{VideoInfo::CS_YUVA422P12, "YUVA422P12"},
    PixelTypeName{VideoInfo::CS_YUVA444P12, "YUVA444P12"},
    PixelTypeName{VideoInfo::CS_YUVA420P14, "YUVA420P14"},
    PixelTypeName{VideoInfo::CS_YUVA422P14, "YUVA422P14"},
    PixelTypeName{VideoInfo::CS_YUVA444P14, "YUVA444P14"},
    PixelTypeName{VideoInfo::CS_YUVA420P16, "YUVA420P16"},
    PixelTypeName{VideoInfo::CS_YUVA422P16, "YUVA422P16"},
    PixelTypeName{VideoInfo::CS_YUVA444P16, "YUVA444P16"},
    PixelTypeName{VideoInfo::CS_YUVA420PS, "YUVA420PS"},
    PixelTypeName{VideoInfo::CS_YUVA422PS, "YUVA422PS"},
    PixelTypeName{VideoInfo::CS_YUVA444PS, "YUVA444PS"},
    PixelTypeName{VideoInfo::CS_RGBAP, "RGBAP"},
    PixelTypeName{VideoInfo::CS_RGBAP10, "RGBAP10"},
    PixelTypeName{VideoInfo::CS_RGBAP12, "RGBAP12"},
    PixelTypeName{VideoInfo::CS_RGBAP14, "RGBAP14"},
    PixelTypeName{VideoInfo::CS_RGBAP16, "RGBAP16"},
    PixelTypeName{VideoInfo::CS_RGBAPS, "RGBAPS"},
    PixelTypeName{VideoInfo::CS_YV24, "YUV444"},
    PixelTypeName{VideoInfo::CS_YV16, "YUV422"},
    PixelTypeName{VideoInfo::CS_YV12, "YUV420"},
    PixelTypeName{VideoInfo::CS_YV411, "YUV411"},
    PixelTypeName{VideoInfo::CS_RGBP, "RGBP8"},
    PixelTypeName{VideoInfo::CS_RGBAP, "RGBAP8"},
    PixelTypeName{VideoInfo::CS_YV24, "YUV444P8"},
    PixelTypeName{VideoInfo::CS_YV16, "YUV422P8"},
    PixelTypeName{VideoInfo::CS_YV12, "YUV420P8"},
    PixelTypeName{VideoInfo::CS_YV411, "YUV411P8"},
    PixelTypeName{VideoInfo::CS_YUVA420, "YUVA420P8"},
    PixelTypeName{VideoInfo::CS_YUVA422, "YUVA422P8"},
    PixelTypeName{VideoInfo::CS_YUVA444, "YUVA444P8"},
};

[[nodiscard]] bool equals_ascii_ignore_case(const std::string_view left, const std::string_view right) noexcept {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    const auto to_upper = [](const char value) {
      return value >= 'a' && value <= 'z' ? static_cast<char>(value - ('a' - 'A')) : value;
    };
    if (to_upper(left[index]) != to_upper(right[index])) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] int pixel_type_from_name(const char* const name) noexcept {
  const std::string_view requested(name);
  for (const auto& candidate : kPixelTypeNames) {
    if (equals_ascii_ignore_case(requested, candidate.name)) {
      return candidate.pixel_type;
    }
  }
  return VideoInfo::CS_UNKNOWN;
}

FillTarget writable_view(PVideoFrame& frame, int plane, IScriptEnvironment* env) {
  return {frame, plane, env};
}
[[nodiscard]] std::uint8_t byte_component(const int value) noexcept {
  return static_cast<std::uint8_t>(value & 0xFF);
}

[[nodiscard]] int max_component_value(const VideoInfo& video_info) {
  const int bits = video_info.BitsPerComponent();
  if (bits <= 0 || bits >= static_cast<int>(std::numeric_limits<int>::digits)) {
    throw std::invalid_argument("BlankClip unsupported integer component size");
  }
  return (1 << bits) - 1;
}

[[nodiscard]] int rgb8_to_component(const int value, const int max_value) noexcept {
  return value * max_value / 255;
}

void fill_planar_component(PVideoFrame& frame, const int plane, const VideoInfo& video_info, const int integer_value,
                           const float float_value, IScriptEnvironment* const env) {
  const auto destination = writable_view(frame, plane, env);
  switch (video_info.ComponentSize()) {
    case 1:
      fill_blank_u8(destination, static_cast<std::uint8_t>(integer_value));
      return;
    case 2:
      fill_blank_u16(destination, static_cast<std::uint16_t>(integer_value));
      return;
    case 4:
      fill_blank_f32(destination, float_value);
      return;
    default:
      env->ThrowError("BlankClip: unsupported planar component size");
  }
}

[[nodiscard]] int legacy_planar_component(const VideoInfo& video_info, const int plane, const ColorValues& colors) {
  const std::uint32_t color = static_cast<std::uint32_t>(colors.packed_color);
  const bool is_yuv = video_info.IsYUV() || video_info.IsYUVA();
  const std::uint32_t color_yuv = colors.mode == ColorMode::yuv ? color : rgb_to_yuv_rec601(color);
  int value{};
  if (is_yuv) {
    switch (plane) {
      case PLANAR_A:
        value = static_cast<int>((color >> 24U) & 0xFFU);
        break;
      case PLANAR_Y:
        value = static_cast<int>((color_yuv >> 16U) & 0xFFU);
        break;
      case PLANAR_U:
        value = static_cast<int>((color_yuv >> 8U) & 0xFFU);
        break;
      case PLANAR_V:
        value = static_cast<int>(color_yuv & 0xFFU);
        break;
      default:
        throw std::invalid_argument("BlankClip unsupported planar YUV plane");
    }
    if (video_info.BitsPerComponent() != 32) {
      value <<= video_info.BitsPerComponent() - 8;
    }
    return value;
  }

  switch (plane) {
    case PLANAR_A:
      value = static_cast<int>((color >> 24U) & 0xFFU);
      break;
    case PLANAR_R:
      value = static_cast<int>((color >> 16U) & 0xFFU);
      break;
    case PLANAR_G:
      value = static_cast<int>((color >> 8U) & 0xFFU);
      break;
    case PLANAR_B:
      value = static_cast<int>(color & 0xFFU);
      break;
    default:
      throw std::invalid_argument("BlankClip unsupported planar RGB plane");
  }
  return video_info.BitsPerComponent() == 32 ? value : rgb8_to_component(value, max_component_value(video_info));
}

[[nodiscard]] float legacy_planar_float(const int plane, const int value) noexcept {
  if (plane == PLANAR_U || plane == PLANAR_V) {
    return static_cast<float>(value - 128) / 255.0F;
  }
  return static_cast<float>(value) / 255.0F;
}

void fill_planar(PVideoFrame& frame, const VideoInfo& video_info, const ColorValues& colors,
                 IScriptEnvironment* const env) {
  const bool is_yuv = video_info.IsYUV() || video_info.IsYUVA();
  const std::array<int, 4> array_order = is_yuv ? std::array<int, 4>{PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A}
                                                : std::array<int, 4>{PLANAR_R, PLANAR_G, PLANAR_B, PLANAR_A};
  const std::array<int, 4> legacy_order = is_yuv ? std::array<int, 4>{PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A}
                                                 : std::array<int, 4>{PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};

  for (int index = 0; index < video_info.NumComponents(); ++index) {
    const int plane =
        colors.is_array ? array_order[static_cast<std::size_t>(index)] : legacy_order[static_cast<std::size_t>(index)];
    if (colors.is_array) {
      fill_planar_component(frame, plane, video_info, colors.integer_components[static_cast<std::size_t>(index)],
                            colors.float_components[static_cast<std::size_t>(index)], env);
      continue;
    }

    const int value = legacy_planar_component(video_info, plane, colors);
    fill_planar_component(frame, plane, video_info, value, legacy_planar_float(plane, value), env);
  }
}

void fill_packed(PVideoFrame& frame, const VideoInfo& video_info, const ColorValues& colors,
                 IScriptEnvironment* const env) {
  const auto destination = writable_view(frame, DEFAULT_PLANE, env);
  const int max_value = max_component_value(video_info);
  const auto component = [&colors](const std::size_t index) {
    return colors.integer_components[index];
  };

  if (video_info.IsYUY2()) {
    std::uint32_t color = static_cast<std::uint32_t>(colors.packed_color);
    if (colors.mode != ColorMode::yuv) {
      color = rgb_to_yuv_rec601(color);
    }
    if (colors.is_array) {
      color = static_cast<std::uint32_t>(std::clamp(component(0), 0, max_value) << 16) |
              static_cast<std::uint32_t>(std::clamp(component(1), 0, max_value) << 8) |
              static_cast<std::uint32_t>(std::clamp(component(2), 0, max_value));
    }
    fill_blank_yuy2(destination, byte_component(static_cast<int>(color >> 16U)),
                    byte_component(static_cast<int>(color >> 8U)), byte_component(static_cast<int>(color)));
    return;
  }

  const std::uint32_t color = static_cast<std::uint32_t>(colors.packed_color);
  if (video_info.IsRGB24()) {
    const int b = colors.is_array ? std::clamp(component(2), 0, max_value) : static_cast<int>(color & 0xFFU);
    const int g = colors.is_array ? std::clamp(component(1), 0, max_value) : static_cast<int>((color >> 8U) & 0xFFU);
    const int r = colors.is_array ? std::clamp(component(0), 0, max_value) : static_cast<int>((color >> 16U) & 0xFFU);
    fill_blank_bgr24(destination, byte_component(b), byte_component(g), byte_component(r));
    return;
  }
  if (video_info.IsRGB32()) {
    const int b = colors.is_array ? std::clamp(component(2), 0, max_value) : static_cast<int>(color & 0xFFU);
    const int g = colors.is_array ? std::clamp(component(1), 0, max_value) : static_cast<int>((color >> 8U) & 0xFFU);
    const int r = colors.is_array ? std::clamp(component(0), 0, max_value) : static_cast<int>((color >> 16U) & 0xFFU);
    const int a = colors.is_array ? std::clamp(component(3), 0, max_value) : static_cast<int>((color >> 24U) & 0xFFU);
    fill_blank_bgr32(destination, byte_component(b), byte_component(g), byte_component(r), byte_component(a));
    return;
  }
  if (video_info.IsRGB48()) {
    const int b = colors.is_array ? std::clamp(component(2), 0, max_value)
                                  : rgb8_to_component(static_cast<int>(color & 0xFFU), max_value);
    const int g = colors.is_array ? std::clamp(component(1), 0, max_value)
                                  : rgb8_to_component(static_cast<int>((color >> 8U) & 0xFFU), max_value);
    const int r = colors.is_array ? std::clamp(component(0), 0, max_value)
                                  : rgb8_to_component(static_cast<int>((color >> 16U) & 0xFFU), max_value);
    fill_blank_bgr48(destination, static_cast<std::uint16_t>(b), static_cast<std::uint16_t>(g),
                     static_cast<std::uint16_t>(r));
    return;
  }
  if (video_info.IsRGB64()) {
    const int b = colors.is_array ? std::clamp(component(2), 0, max_value)
                                  : rgb8_to_component(static_cast<int>(color & 0xFFU), max_value);
    const int g = colors.is_array ? std::clamp(component(1), 0, max_value)
                                  : rgb8_to_component(static_cast<int>((color >> 8U) & 0xFFU), max_value);
    const int r = colors.is_array ? std::clamp(component(0), 0, max_value)
                                  : rgb8_to_component(static_cast<int>((color >> 16U) & 0xFFU), max_value);
    const int a = colors.is_array ? std::clamp(component(3), 0, max_value)
                                  : rgb8_to_component(static_cast<int>((color >> 24U) & 0xFFU), max_value);
    fill_blank_bgr64(destination, static_cast<std::uint16_t>(b), static_cast<std::uint16_t>(g),
                     static_cast<std::uint16_t>(r), static_cast<std::uint16_t>(a));
    return;
  }
  env->ThrowError("BlankClip: unsupported packed pixel format");
}

[[nodiscard]] PVideoFrame create_blank_frame(const VideoInfo& video_info, const ColorValues& colors,
                                             IScriptEnvironment* const env) {
  if (!video_info.HasVideo()) {
    return {};
  }

  PVideoFrame frame = env->NewVideoFrame(video_info);
  AVSMap* const properties = env->getFramePropsRW(frame);
  const bool is_rgb = video_info.IsRGB();
  env->propSetInt(properties, "_Matrix", is_rgb ? kMatrixRgb : kMatrixSt170M, PROPAPPENDMODE_REPLACE);
  env->propSetInt(properties, "_ColorRange", is_rgb ? kColorRangeFull : kColorRangeLimited, PROPAPPENDMODE_REPLACE);

  if (video_info.IsPlanar()) {
    fill_planar(frame, video_info, colors, env);
  } else {
    fill_packed(frame, video_info, colors, env);
  }
  return frame;
}

void parse_color_array(const AVSValue& argument, const VideoInfo& video_info, ColorValues& colors,
                       IScriptEnvironment* const env) {
  if (!argument.Defined()) {
    return;
  }
  if (!argument.IsArray()) {
    env->ThrowError("BlankClip: colors must be an array");
  }
  const int color_count = argument.ArraySize();
  if (color_count > 4) {
    env->ThrowError("BlankClip: 'colors' size %d cannot exceed 4 components", color_count);
  }
  const int component_count = video_info.NumComponents();
  if (color_count < component_count) {
    env->ThrowError("BlankClip: 'colors' size %d is less than component count %d", color_count, component_count);
  }

  const int component_size = video_info.ComponentSize();
  const int bits_per_component = video_info.BitsPerComponent();
  const int count = color_count;
  for (int index = 0; index < count; ++index) {
    const float value = argument[index].AsFloatf(0.0F);
    if (component_size == 4) {
      colors.float_components[static_cast<std::size_t>(index)] = value;
      continue;
    }

    // AviSynth's integer-color path rounds through an int. Check the conversion
    // boundary first so malformed script values cannot cause undefined behavior.
    const double rounded_value = value + 0.5F;
    if (!std::isfinite(value) || rounded_value < static_cast<double>(std::numeric_limits<int>::lowest()) ||
        rounded_value > static_cast<double>(std::numeric_limits<int>::max())) {
      env->ThrowError("BlankClip: colors must contain finite, representable values");
    }
    const int rounded = static_cast<int>(rounded_value);
    if (rounded >= (1 << bits_per_component) || rounded < 0) {
      env->ThrowError("BlankClip: invalid color value (%d) for %d-bit video format", rounded, bits_per_component);
    }
    colors.integer_components[static_cast<std::size_t>(index)] = rounded;
  }
  colors.is_array = true;
}

[[nodiscard]] VideoInfo default_video_info() {
  VideoInfo video_info{};
  video_info.fps_denominator = 1;
  video_info.fps_numerator = 24;
  video_info.height = 480;
  video_info.pixel_type = VideoInfo::CS_BGR32;
  video_info.num_frames = 240;
  video_info.width = 640;
  video_info.audio_samples_per_second = 44100;
  video_info.nchannels = 1;
  video_info.num_audio_samples = 44100 * 10;
  video_info.sample_type = SAMPLE_INT16;
  video_info.SetFieldBased(false);
  return video_info;
}

void set_sample_type(const AVSValue& argument, VideoInfo& video_info, const VideoInfo& default_video_info,
                     IScriptEnvironment* const env) {
  if (argument.IsBool()) {
    video_info.sample_type = argument.AsBool() ? SAMPLE_INT16 : SAMPLE_FLOAT;
    return;
  }
  if (!argument.IsString()) {
    video_info.sample_type = default_video_info.sample_type;
    return;
  }

  const std::string_view sample_type(argument.AsString());
  if (equals_ascii_ignore_case(sample_type, "8bit")) {
    video_info.sample_type = SAMPLE_INT8;
  } else if (equals_ascii_ignore_case(sample_type, "16bit")) {
    video_info.sample_type = SAMPLE_INT16;
  } else if (equals_ascii_ignore_case(sample_type, "24bit")) {
    video_info.sample_type = SAMPLE_INT24;
  } else if (equals_ascii_ignore_case(sample_type, "32bit")) {
    video_info.sample_type = SAMPLE_INT32;
  } else if (equals_ascii_ignore_case(sample_type, "float")) {
    video_info.sample_type = SAMPLE_FLOAT;
  } else {
    env->ThrowError("BlankClip: sample_type must be \"8bit\", \"16bit\", \"24bit\", \"32bit\" or \"float\"");
  }
}

} // namespace
AVSValue __cdecl create_blank_clip(AVSValue arguments, void*, IScriptEnvironment* env) {
  try {
    return make_blank_clip(std::move(arguments), env);
  } catch (const AvisynthError&) {
    throw;
  } catch (const std::exception& error) {
    env->ThrowError("BlankClip: %s", error.what());
  } catch (...) {
    env->ThrowError("BlankClip: unexpected exception");
  }
  return {};
}

PClip make_blank_clip(AVSValue arguments, IScriptEnvironment* const env) {
  VideoInfo default_info = default_video_info();
  bool parity = false;

  const AVSValue first_clip_argument = arguments[0];
  if (first_clip_argument.Defined() && first_clip_argument.ArraySize() == 1 && !arguments[12].Defined()) {
    const PClip template_clip = first_clip_argument[0].AsClip();
    default_info = template_clip->GetVideoInfo();
    parity = template_clip->GetParity(0);
  } else if (first_clip_argument.Defined() && first_clip_argument.ArraySize() != 0) {
    env->ThrowError("BlankClip: Only 1 Template clip allowed.");
  } else if (arguments[12].Defined()) {
    const PClip template_clip = arguments[12].AsClip();
    default_info = template_clip->GetVideoInfo();
    parity = template_clip->GetParity(0);
  }

  VideoInfo video_info{};
  const bool default_has_video = default_info.HasVideo();
  const bool default_has_audio = default_info.HasAudio();
  if (!default_has_video) {
    default_info.fps_numerator = 24;
    default_info.fps_denominator = 1;
    default_info.num_frames = 240;
    if (arguments[2].Defined() || arguments[3].Defined() || arguments[4].Defined()) {
      default_info.width = 640;
      default_info.height = 480;
      default_info.pixel_type = VideoInfo::CS_BGR32;
      default_info.SetFieldBased(false);
      parity = false;
    }
  }
  if (!default_has_audio && (arguments[7].Defined() || arguments[8].Defined() || arguments[9].Defined())) {
    default_info.audio_samples_per_second = 44100;
    default_info.nchannels = 1;
    default_info.sample_type = SAMPLE_INT16;
  }

  video_info.width = arguments[2].AsInt(default_info.width);
  video_info.height = arguments[3].AsInt(default_info.height);
  if (arguments[4].Defined()) {
    const int pixel_type = pixel_type_from_name(arguments[4].AsString());
    if (pixel_type == VideoInfo::CS_UNKNOWN) {
      env->ThrowError("BlankClip: pixel_type must be \"RGB32\", \"RGB24\", \"YV12\", \"YV24\", "
                      "\"YV16\", \"Y8\", \n"
                      "\"YUV420P?\",\"YUV422P?\",\"YUV444P?\",\"Y?\",\n"
                      "\"RGB48\",\"RGB64\",\"RGBP\",\"RGBP?\",\n"
                      "\"YV411\" or \"YUY2\"");
    }
    video_info.pixel_type = pixel_type;
  } else {
    video_info.pixel_type = default_info.pixel_type;
  }
  if (video_info.pixel_type == VideoInfo::CS_UNKNOWN) {
    video_info.pixel_type = VideoInfo::CS_BGR32;
  }

  double fps = arguments[5].AsDblDef(static_cast<double>(default_info.fps_numerator));
  if (!std::isfinite(fps)) {
    env->ThrowError("BlankClip: fps must be finite");
  }
  if (arguments[5].Defined() && !arguments[6].Defined()) {
    unsigned denominator = 1;
    while (fps < 16777216.0 && denominator < 16777216U) {
      fps *= 2.0;
      denominator *= 2U;
    }
    video_info.SetFPS(static_cast<int>(fps + 0.5), denominator);
  } else {
    video_info.SetFPS(static_cast<int>(fps + 0.5), arguments[6].AsInt(static_cast<int>(default_info.fps_denominator)));
  }

  video_info.image_type = default_info.image_type;
  video_info.audio_samples_per_second = arguments[7].AsInt(default_info.audio_samples_per_second);
  if (arguments[8].IsBool()) {
    video_info.nchannels = arguments[8].AsBool() ? 2 : 1;
  } else if (arguments[8].IsInt()) {
    video_info.nchannels = arguments[8].AsInt();
  } else {
    video_info.nchannels = default_info.nchannels;
  }
  set_sample_type(arguments[9], video_info, default_info, env);

  if (!default_has_video && default_has_audio) {
    const int64_t denominator =
        static_cast<int64_t>(video_info.fps_denominator) * default_info.audio_samples_per_second;
    default_info.num_frames =
        static_cast<int>((default_info.num_audio_samples * video_info.fps_numerator + denominator - 1) / denominator);
  }

  video_info.num_frames = arguments[1].AsInt(default_info.num_frames);
  ++video_info.width;
  video_info.num_audio_samples = video_info.AudioSamplesFromFrames(video_info.num_frames);
  --video_info.width;

  ColorValues colors;
  colors.packed_color = arguments[10].AsInt(0);
  if (arguments[11].Defined()) {
    if (colors.packed_color != 0) {
      env->ThrowError("BlankClip: color and color_yuv are mutually exclusive");
    }
    if (!video_info.IsYUV() && !video_info.IsYUVA()) {
      env->ThrowError("BlankClip: color_yuv only valid for YUV color spaces");
    }
    colors.packed_color = arguments[11].AsInt();
    colors.mode = ColorMode::yuv;
  }
  if (arguments.ArraySize() >= 14) {
    parse_color_array(arguments[13], video_info, colors, env);
  }

  PClip clip = new StaticImage(video_info, create_blank_frame(video_info, colors, env), parity);
  const AVSValue on_cpu_arguments[2]{clip, 1};
  return new SingleFrame(env->Invoke("OnCPU", AVSValue(on_cpu_arguments, 2)).AsClip());
}

} // namespace aif::filters::blank_clip
