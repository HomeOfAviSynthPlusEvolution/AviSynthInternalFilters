// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2002 Ben Rudiak-Gould et al.
// Copyright (C) 2026 AviSynthPlus-IF contributors
// Derived from AviSynthPlus avs_core/filters/transform.cpp.

#include "add_borders.h"

#include "crop.h"
#include "kernel_adapter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace aif::filters::crop {
namespace {

[[nodiscard]] std::uint32_t rgb_to_yuv_rec601(const std::uint32_t rgb) noexcept {
  constexpr int cyb = static_cast<int>(0.114 * 219 / 255 * 65536 + 0.5);
  constexpr int cyg = static_cast<int>(0.587 * 219 / 255 * 65536 + 0.5);
  constexpr int cyr = static_cast<int>(0.299 * 219 / 255 * 65536 + 0.5);
  const int blue = static_cast<int>(rgb & 0xFFU);
  const int green = static_cast<int>((rgb >> 8U) & 0xFFU);
  const int red = static_cast<int>((rgb >> 16U) & 0xFFU);
  const int y = (cyb * blue + cyg * green + cyr * red + 0x108000) >> 16;
  const int scaled_y = (y - 16) * static_cast<int>(255.0 / 219.0 * 65536 + 0.5);
  const int b_y = (blue << 16) - scaled_y;
  const int u = std::clamp(((b_y >> 10) * static_cast<int>(1 / 2.018 * 1024 + 0.5) + 0x800000 + 32768) >> 16, 0, 255);
  const int r_y = (red << 16) - scaled_y;
  const int v = std::clamp(((r_y >> 10) * static_cast<int>(1 / 1.596 * 1024 + 0.5) + 0x800000 + 32768) >> 16, 0, 255);
  return (rgb & 0xFF000000U) | (static_cast<std::uint32_t>(y) << 16U) | (static_cast<std::uint32_t>(u) << 8U) |
         static_cast<std::uint32_t>(v);
}

[[nodiscard]] std::array<std::byte, 4> native_sample(const std::uint8_t color, const bool full_scale,
                                                     const int bits_per_component, const bool chroma,
                                                     const int component_size) {
  std::array<std::byte, 4> result{};
  if (component_size == 1) {
    result[0] = static_cast<std::byte>(color);
  } else if (component_size == 2) {
    const auto value = static_cast<std::uint16_t>(
        full_scale ? (static_cast<unsigned int>(color) * ((1U << bits_per_component) - 1U)) / 255U
                   : static_cast<unsigned int>(color) << (bits_per_component - 8));
    std::memcpy(result.data(), &value, sizeof(value));
  } else if (component_size == 4) {
    const float value = chroma ? (static_cast<int>(color) - 128) / 255.0F : color / 255.0F;
    std::memcpy(result.data(), &value, sizeof(value));
  } else {
    throw std::invalid_argument("AddBorders unsupported component size");
  }
  return result;
}

enum class TransientResamplerKind {
  point,
  bilinear,
  bicubic,
  lanczos,
  lanczos4,
  blackman,
  spline16,
  spline36,
  spline64,
  gauss,
  sinc,
  sin_power,
  sinc_lin2,
  user_defined2,
};

struct TransientResampler {
  TransientResamplerKind kind{};
  double support{};
  AVSValue param1{};
  AVSValue param2{};
  AVSValue param3{};
};

[[nodiscard]] bool equal_name(const std::string_view left, const std::string_view right) noexcept {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    const auto lower = [](const char value) noexcept {
      return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
    };
    if (lower(left[index]) != lower(right[index])) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] int bounded_integer(const AVSValue& value, const float default_value, const int minimum,
                                  const int maximum) {
  return std::clamp(static_cast<int>(value.AsFloat(default_value)), minimum, maximum);
}

[[nodiscard]] double gaussian_support(const double parameter, const double base, double support) {
  const double p = std::clamp(parameter, 0.01, 100.0);
  const double b = std::clamp(base, 1.5, 3.5);
  if (support == 0.0) {
    support = std::sqrt(4.6 / ((p * 0.1) * std::log(b)));
  }
  return std::clamp(support, 0.1, 150.0);
}

[[nodiscard]] TransientResampler resolve_transient_resampler(const AVSValue& resample, const AVSValue& param1,
                                                             const AVSValue& param2, const AVSValue& param3,
                                                             IScriptEnvironment* env) {
  const char* const name = resample.AsString("gauss");
  const std::string_view name_view = name == nullptr ? std::string_view{} : name;

  if (equal_name(name_view, "point")) {
    return {TransientResamplerKind::point, 0.0, param1, param2, param3};
  }
  if (equal_name(name_view, "bilinear")) {
    return {TransientResamplerKind::bilinear, 1.0, param1, param2, param3};
  }
  if (equal_name(name_view, "bicubic")) {
    return {TransientResamplerKind::bicubic, 2.0, param1, param2, param3};
  }
  if (equal_name(name_view, "lanczos")) {
    return {TransientResamplerKind::lanczos, static_cast<double>(bounded_integer(param1, 3.0F, 1, 100)), param1, param2,
            param3};
  }
  if (equal_name(name_view, "lanczos4")) {
    return {TransientResamplerKind::lanczos4, 4.0, param1, param2, param3};
  }
  if (equal_name(name_view, "blackman")) {
    return {TransientResamplerKind::blackman, static_cast<double>(bounded_integer(param1, 4.0F, 1, 100)), param1,
            param2, param3};
  }
  if (equal_name(name_view, "spline16")) {
    return {TransientResamplerKind::spline16, 2.0, param1, param2, param3};
  }
  if (equal_name(name_view, "spline36")) {
    return {TransientResamplerKind::spline36, 3.0, param1, param2, param3};
  }
  if (equal_name(name_view, "spline64")) {
    return {TransientResamplerKind::spline64, 4.0, param1, param2, param3};
  }
  if (equal_name(name_view, "gauss")) {
    const double parameter = param1.AsDblDef(10.0);
    const double base = param2.AsDblDef(2.718281828);
    const double support = param3.AsDblDef(0.0);
    return {TransientResamplerKind::gauss, gaussian_support(parameter, base, support), AVSValue(parameter),
            AVSValue(base), AVSValue(support)};
  }
  if (equal_name(name_view, "sinc")) {
    return {TransientResamplerKind::sinc, static_cast<double>(bounded_integer(param1, 4.0F, 1, 150)), param1, param2,
            param3};
  }
  if (equal_name(name_view, "sinpow")) {
    return {TransientResamplerKind::sin_power, 2.0, param1, param2, param3};
  }
  if (equal_name(name_view, "sinclin2")) {
    return {TransientResamplerKind::sinc_lin2, static_cast<double>(bounded_integer(param1, 15.0F, 1, 30)), param1,
            param2, param3};
  }
  if (equal_name(name_view, "userdefined2")) {
    return {TransientResamplerKind::user_defined2, std::clamp(param3.AsDblDef(2.3), 1.5, 15.0), param1, param2, param3};
  }

  env->ThrowError("AddBorders: unknown resampler name: %s", name_view.data());
  return {};
}

[[nodiscard]] const char* transient_placement(const PClip& clip, const VideoInfo& vi, IScriptEnvironment* env) {
  if (!vi.IsYV411() && !vi.Is420() && !vi.Is422()) {
    return "center";
  }

  const PVideoFrame frame = clip->GetFrame(0, env);
  const AVSMap* const properties = env->getFramePropsRO(frame);
  if (properties == nullptr || env->propNumElements(properties, "_ChromaLocation") == 0) {
    return "center";
  }

  switch (static_cast<int>(env->propGetIntSaturated(properties, "_ChromaLocation", 0, nullptr))) {
    case 0:
      return "left";
    case 1:
      return "center";
    case 2:
      return "top_left";
    case 3:
      return "top";
    case 4:
      return "bottom_left";
    case 5:
      return "bottom";
    case 6:
      return "dv";
    default:
      return "center";
  }
}

[[nodiscard]] PClip invoke_transient_resize(const TransientResampler& resampler, PClip clip, const int width,
                                            const int height, const int force, const char* const placement,
                                            IScriptEnvironment* env) {
  std::array<AVSValue, 13> arguments{};
  arguments[0] = AVSValue(clip);
  arguments[1] = AVSValue(width);
  arguments[2] = AVSValue(height);

  const auto invoke = [&](const char* const name, const int count) {
    return env->Invoke(name, AVSValue(arguments.data(), count)).AsClip();
  };
  const auto set_common_tail = [&](const int force_index, const int keep_center_index, const int placement_index) {
    arguments[force_index] = AVSValue(force);
    arguments[keep_center_index] = AVSValue(true);
    arguments[placement_index] = AVSValue(placement);
  };

  switch (resampler.kind) {
    case TransientResamplerKind::point:
      set_common_tail(7, 8, 9);
      return invoke("PointResize", 10);
    case TransientResamplerKind::bilinear:
      set_common_tail(7, 8, 9);
      return invoke("BilinearResize", 10);
    case TransientResamplerKind::bicubic:
      arguments[3] = resampler.param1;
      arguments[4] = resampler.param2;
      set_common_tail(9, 10, 11);
      return invoke("BicubicResize", 12);
    case TransientResamplerKind::lanczos:
      arguments[7] = AVSValue(static_cast<int>(resampler.support));
      set_common_tail(8, 9, 10);
      return invoke("LanczosResize", 11);
    case TransientResamplerKind::lanczos4:
      set_common_tail(7, 8, 9);
      return invoke("Lanczos4Resize", 10);
    case TransientResamplerKind::blackman:
      arguments[7] = AVSValue(static_cast<int>(resampler.support));
      set_common_tail(8, 9, 10);
      return invoke("BlackmanResize", 11);
    case TransientResamplerKind::spline16:
      set_common_tail(7, 8, 9);
      return invoke("Spline16Resize", 10);
    case TransientResamplerKind::spline36:
      set_common_tail(7, 8, 9);
      return invoke("Spline36Resize", 10);
    case TransientResamplerKind::spline64:
      set_common_tail(7, 8, 9);
      return invoke("Spline64Resize", 10);
    case TransientResamplerKind::gauss:
      arguments[7] = resampler.param1;
      arguments[8] = resampler.param2;
      arguments[9] = resampler.param3;
      set_common_tail(10, 11, 12);
      return invoke("GaussResize", 13);
    case TransientResamplerKind::sinc:
      arguments[7] = AVSValue(static_cast<int>(resampler.support));
      set_common_tail(8, 9, 10);
      return invoke("SincResize", 11);
    case TransientResamplerKind::sin_power:
      arguments[7] = resampler.param1;
      set_common_tail(8, 9, 10);
      return invoke("SinPowerResize", 11);
    case TransientResamplerKind::sinc_lin2:
      arguments[7] = AVSValue(static_cast<int>(resampler.support));
      set_common_tail(8, 9, 10);
      return invoke("SincLin2Resize", 11);
    case TransientResamplerKind::user_defined2:
      arguments[3] = resampler.param1;
      arguments[4] = resampler.param2;
      arguments[5] = resampler.param3;
      set_common_tail(10, 11, 12);
      return invoke("UserDefined2Resize", 13);
  }

  throw std::logic_error("AddBorders: invalid transient resampler");
}

} // namespace

AddBorders::AddBorders(const int left, const int top, const int right, const int bottom, const int color,
                       const bool color_is_yuv, PClip child, IScriptEnvironment* env)
    : GenericVideoFilter(std::move(child)), left_(left), top_(top), right_(right), bottom_(bottom),
      color_(static_cast<std::uint32_t>(color)), color_is_yuv_(color_is_yuv) {
  is_yuv_ = vi.IsYUV() || vi.IsYUVA();
  is_planar_rgb_ = vi.IsPlanarRGB() || vi.IsPlanarRGBA();
  if (is_yuv_) {
    if (vi.NumComponents() > 1) {
      xsub_ = vi.GetPlaneWidthSubsampling(PLANAR_U);
      ysub_ = vi.GetPlaneHeightSubsampling(PLANAR_U);
    }
    validate_yuv_alignment(env);
  } else if (!is_planar_rgb_) {
    std::swap(top_, bottom_); // Packed RGB storage is bottom-up.
  }

  if (!vi.IsPlanar() && !vi.IsYUY2() && !vi.IsRGB24() && !vi.IsRGB32() && !vi.IsRGB48() && !vi.IsRGB64()) {
    env->ThrowError("AddBorders: unsupported pixel format");
  }
  vi.width += left_ + right_;
  vi.height += top_ + bottom_;
}

PVideoFrame __stdcall AddBorders::GetFrame(const int n, IScriptEnvironment* env) {
  const PVideoFrame source = child->GetFrame(n, env);
  PVideoFrame destination = env->NewVideoFrameP(vi, &source);
  if (vi.IsPlanar()) {
    add_planar_borders(source, destination, env);
  } else {
    add_packed_borders(source, destination, env);
  }
  return destination;
}

int __stdcall AddBorders::SetCacheHints(const int cachehints, const int) {
  return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
}

void AddBorders::validate_yuv_alignment(IScriptEnvironment* env) const {
  const int xmask = (1 << xsub_) - 1;
  const int ymask = (1 << ysub_) - 1;
  if (left_ & xmask) {
    env->ThrowError("AddBorders: YUV image can only add by Mod %d (left side).", xmask + 1);
  }
  if (right_ & xmask) {
    env->ThrowError("AddBorders: YUV image can only add by Mod %d (right side).", xmask + 1);
  }
  if (top_ & ymask) {
    env->ThrowError("AddBorders: YUV image can only add by Mod %d (top).", ymask + 1);
  }
  if (bottom_ & ymask) {
    env->ThrowError("AddBorders: YUV image can only add by Mod %d (bottom).", ymask + 1);
  }
}

void AddBorders::add_planar_borders(const PVideoFrame& source, PVideoFrame& destination,
                                    IScriptEnvironment* env) const {
  const std::uint32_t color = is_yuv_ && !color_is_yuv_ ? rgb_to_yuv_rec601(color_) : color_;
  const std::array<std::uint8_t, 4> yuv_colors = {
      static_cast<std::uint8_t>((color >> 16U) & 0xFFU),
      static_cast<std::uint8_t>((color >> 8U) & 0xFFU),
      static_cast<std::uint8_t>(color & 0xFFU),
      static_cast<std::uint8_t>((color >> 24U) & 0xFFU),
  };
  const std::array<std::uint8_t, 4> rgb_colors = {yuv_colors[1], yuv_colors[2], yuv_colors[0], yuv_colors[3]};
  const std::array<int, 4> yuv_planes = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
  const std::array<int, 4> rgb_planes = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
  const auto& planes = is_yuv_ ? yuv_planes : rgb_planes;
  const auto& colors = is_yuv_ ? yuv_colors : rgb_colors;
  const int component_size = vi.ComponentSize();

  for (int index = 0; index < vi.NumComponents(); ++index) {
    const int plane = planes[static_cast<std::size_t>(index)];
    const int plane_xsub = vi.GetPlaneWidthSubsampling(plane);
    const int plane_ysub = vi.GetPlaneHeightSubsampling(plane);
    const bool chroma = plane == PLANAR_U || plane == PLANAR_V;
    const auto sample =
        native_sample(colors[static_cast<std::size_t>(index)], !is_yuv_, vi.BitsPerComponent(), chroma, component_size);
    apply(source, destination, plane, (left_ >> plane_xsub) * component_size, top_ >> plane_ysub, sample.data(),
          component_size, env);
  }
}

void AddBorders::add_packed_borders(const PVideoFrame& source, PVideoFrame& destination,
                                    IScriptEnvironment* env) const {
  std::array<std::byte, 8> pattern{};
  std::size_t pattern_size = 0;
  if (vi.IsYUY2()) {
    const auto color = color_is_yuv_ ? color_ : rgb_to_yuv_rec601(color_);
    // Preserve AviSynthPlus's legacy 32-bit packing, including the alpha-byte
    // contribution and its intentional unsigned overflow.
    const std::uint32_t packed =
        (color >> 16U) * 0x010001U + ((color >> 8U) & 0xFFU) * 0x0100U + (color & 0xFFU) * 0x01000000U;
    std::memcpy(pattern.data(), &packed, sizeof(packed));
    pattern_size = 4;
  } else if (vi.IsRGB24()) {
    pattern[0] = static_cast<std::byte>(color_ & 0xFFU);
    pattern[1] = static_cast<std::byte>((color_ >> 8U) & 0xFFU);
    pattern[2] = static_cast<std::byte>((color_ >> 16U) & 0xFFU);
    pattern_size = 3;
  } else if (vi.IsRGB32()) {
    std::memcpy(pattern.data(), &color_, sizeof(color_));
    pattern_size = 4;
  } else if (vi.IsRGB48() || vi.IsRGB64()) {
    const std::array<std::uint8_t, 4> colors = {
        static_cast<std::uint8_t>(color_ & 0xFFU),
        static_cast<std::uint8_t>((color_ >> 8U) & 0xFFU),
        static_cast<std::uint8_t>((color_ >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((color_ >> 24U) & 0xFFU),
    };
    const int components = vi.IsRGB64() ? 4 : 3;
    for (int index = 0; index < components; ++index) {
      const auto sample = native_sample(colors[static_cast<std::size_t>(index)], true, 16, false, 2);
      std::memcpy(pattern.data() + index * 2, sample.data(), 2);
    }
    pattern_size = static_cast<std::size_t>(components * 2);
  }

  apply(source, destination, DEFAULT_PLANE, vi.BytesFromPixels(left_), top_, pattern.data(), int(pattern_size), env);
}
namespace {
struct TransientBarSection {
  int extent_outer_horiz{};
  int extent_inner_horiz{};
  int extent_outer_vert{};
  int extent_inner_vert{};
  int crop_x{};
  int crop_y{};
  int target_x{};
  int target_y{};
  int target_width{};
  int target_height{};
  int src_x{};
  int src_y{};
  int src_width{};
  int src_height{};
  int force{};
};

struct TransientBarTemplate {
  int side{};
  int direction{};
};

[[nodiscard]] PClip add_border_post_process(PClip child, const int left, const int top, const int right,
                                            const int bottom, const AVSValue& resample, const AVSValue& param1,
                                            const AVSValue& param2, const AVSValue& param3, const AVSValue& radius,
                                            const char* const placement, IScriptEnvironment* env) {
  const int radius_value = radius.AsInt(0);
  if (radius_value == 0) {
    return child;
  }

  const bool both_sides = radius_value > 0;
  int filtering_radius = radius_value < 0 ? -radius_value : radius_value;
  const VideoInfo& vi = child->GetVideoInfo();
  const TransientResampler resampler = resolve_transient_resampler(resample, param1, param2, param3, env);

  const bool grey = vi.IsY();
  const bool is_planar_rgb = vi.IsPlanarRGB() || vi.IsPlanarRGBA();
  const int shift_w = vi.IsPlanar() && !grey && !is_planar_rgb ? vi.GetPlaneWidthSubsampling(PLANAR_U) : 0;
  const int shift_h = vi.IsPlanar() && !grey && !is_planar_rgb ? vi.GetPlaneHeightSubsampling(PLANAR_U) : 0;
  const int shift = std::max(shift_w, shift_h);
  const int chroma_unit = 1 << shift;

  filtering_radius = (filtering_radius + chroma_unit - 1) & ~(chroma_unit - 1);

  constexpr int minimum_filtering_extent = 10;
  int filtering_width =
      std::max(minimum_filtering_extent, filtering_radius + static_cast<int>(std::ceil(resampler.support)));
  filtering_width = (filtering_width + chroma_unit - 1) & ~(chroma_unit - 1);

  const int original_width = vi.width - left - right;
  const int original_height = vi.height - top - bottom;
  constexpr std::array templates = {
      TransientBarTemplate{0, 1}, TransientBarTemplate{1, 1}, TransientBarTemplate{2, 2}, TransientBarTemplate{3, 2},
      TransientBarTemplate{4, 3}, TransientBarTemplate{5, 3}, TransientBarTemplate{6, 3}, TransientBarTemplate{7, 3},
  };

  std::vector<TransientBarSection> bars;
  bars.reserve(templates.size());
  for (const TransientBarTemplate& bar_template : templates) {
    TransientBarSection bar{};
    const bool filter_horizontally = bar_template.direction == 1 || bar_template.direction == 3;
    const bool filter_vertically = bar_template.direction == 2 || bar_template.direction == 3;

    int border_size = 0;
    int border_size2 = 0;
    switch (bar_template.side) {
      case 0:
        border_size = left;
        break;
      case 1:
        border_size = right;
        break;
      case 2:
        border_size2 = top;
        break;
      case 3:
        border_size2 = bottom;
        break;
      case 4:
        border_size = left;
        border_size2 = top;
        break;
      case 5:
        border_size = right;
        border_size2 = top;
        break;
      case 6:
        border_size = left;
        border_size2 = bottom;
        break;
      case 7:
        border_size = right;
        border_size2 = bottom;
        break;
      default:
        throw std::logic_error("AddBorders: invalid transient bar side");
    }

    int safe_filtering_radius = filtering_radius;
    if (border_size > 0) {
      bar.extent_outer_horiz = std::min(filtering_width, border_size);
      bar.extent_inner_horiz = 2 * filtering_width - bar.extent_outer_horiz;
    }
    if (border_size2 > 0) {
      bar.extent_outer_vert = std::min(filtering_width, border_size2);
      bar.extent_inner_vert = 2 * filtering_width - bar.extent_outer_vert;
    }

    if (filter_horizontally) {
      safe_filtering_radius = std::min(safe_filtering_radius, border_size);
      bar.extent_inner_horiz = std::min(bar.extent_inner_horiz, original_width);
    }
    if (filter_vertically) {
      safe_filtering_radius = std::min(safe_filtering_radius, border_size2);
      bar.extent_inner_vert = std::min(bar.extent_inner_vert, original_height);
    }

    const int safe_inner_radius = both_sides ? safe_filtering_radius : 0;
    const int safe_inner_radius_horiz = std::min(bar.extent_inner_horiz, safe_inner_radius);
    const int safe_inner_radius_vert = std::min(bar.extent_inner_vert, safe_inner_radius);
    bar.force = bar_template.direction;

    if (filter_horizontally && filter_vertically) {
      bar.target_width = bar.extent_outer_horiz + bar.extent_inner_horiz;
      bar.target_height = bar.extent_outer_vert + bar.extent_inner_vert;
      bar.src_width = safe_filtering_radius + safe_inner_radius_horiz;
      bar.src_height = safe_filtering_radius + safe_inner_radius_vert;

      if (bar_template.side == 4 || bar_template.side == 6) {
        bar.crop_x = left - bar.extent_outer_horiz;
        bar.target_x = left - safe_filtering_radius;
        bar.src_x = bar.extent_outer_horiz - safe_filtering_radius;
      } else {
        bar.crop_x = left + original_width - bar.extent_inner_horiz;
        bar.target_x = left + original_width - safe_inner_radius_horiz;
        bar.src_x = bar.extent_inner_horiz - safe_inner_radius_horiz;
      }

      if (bar_template.side == 4 || bar_template.side == 5) {
        bar.crop_y = top - bar.extent_outer_vert;
        bar.target_y = top - safe_filtering_radius;
        bar.src_y = bar.extent_outer_vert - safe_filtering_radius;
      } else {
        bar.crop_y = top + original_height - bar.extent_inner_vert;
        bar.target_y = top + original_height - safe_inner_radius_vert;
        bar.src_y = bar.extent_inner_vert - safe_inner_radius_vert;
      }
    } else if (filter_horizontally) {
      bar.target_width = bar.extent_outer_horiz + bar.extent_inner_horiz;
      bar.target_height = original_height;
      bar.src_width = safe_filtering_radius + safe_inner_radius_horiz;
      bar.src_height = original_height;

      if (bar_template.side == 0) {
        bar.crop_x = left - bar.extent_outer_horiz;
        bar.crop_y = top;
        bar.target_x = left - safe_filtering_radius;
        bar.target_y = top;
        bar.src_x = bar.extent_outer_horiz - safe_filtering_radius;
      } else {
        bar.crop_x = left + original_width - bar.extent_inner_horiz;
        bar.crop_y = top;
        bar.target_x = left + original_width - safe_inner_radius_horiz;
        bar.target_y = top;
        bar.src_x = bar.extent_inner_horiz - safe_inner_radius_horiz;
      }
      bar.src_y = 0;
    } else {
      bar.target_width = original_width;
      bar.target_height = bar.extent_outer_vert + bar.extent_inner_vert;
      bar.src_width = original_width;
      bar.src_height = safe_filtering_radius + safe_inner_radius_vert;

      if (bar_template.side == 2) {
        bar.crop_x = left;
        bar.crop_y = top - bar.extent_outer_vert;
        bar.target_x = left;
        bar.target_y = top - safe_filtering_radius;
        bar.src_y = bar.extent_outer_vert - safe_filtering_radius;
      } else {
        bar.crop_x = left;
        bar.crop_y = top + original_height - bar.extent_inner_vert;
        bar.target_x = left;
        bar.target_y = top + original_height - safe_inner_radius_vert;
        bar.src_y = bar.extent_inner_vert - safe_inner_radius_vert;
      }
      bar.src_x = 0;
    }

    const bool dimensions_valid =
        (!filter_horizontally || bar.target_width > 0) && (!filter_vertically || bar.target_height > 0);
    if (dimensions_valid) {
      bars.push_back(bar);
    }
  }

  if (bars.empty()) {
    return child;
  }

  std::vector<AVSValue> overlay_clips;
  std::vector<AVSValue> positions;
  overlay_clips.reserve(bars.size());
  positions.reserve(bars.size() * 6U);
  for (const TransientBarSection& bar : bars) {
    const PClip cropped = make_crop(child, bar.crop_x, bar.crop_y, bar.target_width, bar.target_height, false, env);
    overlay_clips.emplace_back(
        invoke_transient_resize(resampler, cropped, bar.target_width, bar.target_height, bar.force, placement, env));
    positions.emplace_back(bar.target_x);
    positions.emplace_back(bar.target_y);
    positions.emplace_back(bar.src_x);
    positions.emplace_back(bar.src_y);
    positions.emplace_back(bar.src_width);
    positions.emplace_back(bar.src_height);
  }

  const std::array<AVSValue, 3> arguments = {
      AVSValue(child),
      AVSValue(overlay_clips.data(), static_cast<int>(overlay_clips.size())),
      AVSValue(positions.data(), static_cast<int>(positions.size())),
  };
  return env->Invoke("MultiOverlay", AVSValue(arguments.data(), 3)).AsClip();
}

} // namespace

AVSValue __cdecl create_add_borders(AVSValue arguments, void*, IScriptEnvironment* env) {
  try {
    PClip clip = arguments[0].AsClip();
    const VideoInfo& input_vi = clip->GetVideoInfo();
    int color = arguments[5].AsInt(0);
    bool color_is_yuv = false;
    if (arguments[6].Defined()) {
      if (color != 0) {
        env->ThrowError("AddBorders: color and color_yuv are mutually exclusive");
      }
      if (!input_vi.IsYUV() && !input_vi.IsYUVA()) {
        env->ThrowError("AddBorders: color_yuv only valid for YUV color spaces");
      }
      color = arguments[6].AsInt();
      color_is_yuv = true;
    }
    const int left = std::max(0, arguments[1].AsInt());
    const int top = std::max(0, arguments[2].AsInt());
    const int right = std::max(0, arguments[3].AsInt());
    const int bottom = std::max(0, arguments[4].AsInt());
    return make_add_borders_with_transient(std::move(clip), left, top, right, bottom, color, color_is_yuv, arguments[7],
                                           arguments[8], arguments[9], arguments[10], arguments[11], env);
  } catch (const AvisynthError&) {
    throw;
  } catch (const std::exception& error) {
    env->ThrowError("AddBorders: %s", error.what());
  } catch (...) {
    env->ThrowError("AddBorders: unexpected exception");
  }
  return {};
}

PClip make_add_borders(PClip clip, const int left, const int top, const int right, const int bottom, const int color,
                       const bool color_is_yuv, IScriptEnvironment* env) {
  return new AddBorders(left, top, right, bottom, color, color_is_yuv, std::move(clip), env);
}

PClip make_add_borders_with_transient(PClip clip, const int left, const int top, const int right, const int bottom,
                                      const int color, const bool color_is_yuv, const AVSValue& resample,
                                      const AVSValue& param1, const AVSValue& param2, const AVSValue& param3,
                                      const AVSValue& radius, IScriptEnvironment* env) {
  const char* placement = "center";
  if (radius.AsInt(0) != 0) {
    placement = transient_placement(clip, clip->GetVideoInfo(), env);
  }
  PClip result = make_add_borders(std::move(clip), left, top, right, bottom, color, color_is_yuv, env);
  return add_border_post_process(std::move(result), left, top, right, bottom, resample, param1, param2, param3, radius,
                                 placement, env);
}

} // namespace aif::filters::crop
