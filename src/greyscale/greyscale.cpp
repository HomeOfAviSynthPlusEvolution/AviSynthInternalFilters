#include "kernel_adapter.h"
// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from AviSynthPlus and the earlier AviSynthPlus-IF adapter.
#include "greyscale.h"
#include <array>
#include <cctype>
#include <optional>
#include <string_view>
#include <utility>
namespace aif::filters::greyscale {
namespace {
enum class ColorRange { full, limited };
enum class RgbLumaMatrix { rgb, bt709, bt470_m, bt470_bg, st170_m, st240_m, bt2020_ncl, bt2020_cl, average };
enum class ParsedMatrix {
  rgb,
  bt709,
  unspecified,
  bt470_m,
  bt470_bg,
  st170_m,
  st240_m,
  ycgco,
  bt2020_ncl,
  bt2020_cl,
  chroma_ncl,
  chroma_cl,
  ictcp,
  average,
};

struct MatrixSelection {
  RgbLumaMatrix matrix;
  ColorRange source_range;
  ColorRange destination_range;
};

[[nodiscard]] bool ascii_iequals(const std::string_view left, const std::string_view right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    const auto left_character = static_cast<unsigned char>(left[index]);
    const auto right_character = static_cast<unsigned char>(right[index]);
    if (std::tolower(left_character) != std::tolower(right_character)) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool is_auto(const std::string_view value) {
  return value.empty() || ascii_iequals(value, "auto");
}

[[nodiscard]] std::optional<ColorRange> parse_range(const std::string_view value) {
  if (ascii_iequals(value, "limited") || ascii_iequals(value, "l")) {
    return ColorRange::limited;
  }
  if (ascii_iequals(value, "full") || ascii_iequals(value, "f")) {
    return ColorRange::full;
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<ColorRange> color_range_from_property(const int value) {
  switch (value) {
    case 0:
      return ColorRange::full;
    case 1:
      return ColorRange::limited;
    default:
      return std::nullopt;
  }
}

[[nodiscard]] std::optional<ParsedMatrix> parse_new_matrix(const std::string_view value) {
  constexpr std::array entries = {
      std::pair{"rgb", ParsedMatrix::rgb},
      std::pair{"709", ParsedMatrix::bt709},
      std::pair{"unspec", ParsedMatrix::unspecified},
      std::pair{"170m", ParsedMatrix::st170_m},
      std::pair{"240m", ParsedMatrix::st240_m},
      std::pair{"470bg", ParsedMatrix::bt470_bg},
      std::pair{"fcc", ParsedMatrix::bt470_m},
      std::pair{"470m", ParsedMatrix::bt470_m},
      std::pair{"ycgco", ParsedMatrix::ycgco},
      std::pair{"2020ncl", ParsedMatrix::bt2020_ncl},
      std::pair{"2020cl", ParsedMatrix::bt2020_cl},
      std::pair{"chromacl", ParsedMatrix::chroma_cl},
      std::pair{"chromancl", ParsedMatrix::chroma_ncl},
      std::pair{"ictcp", ParsedMatrix::ictcp},
      std::pair{"601", ParsedMatrix::bt470_bg},
      std::pair{"2020", ParsedMatrix::bt2020_ncl},
  };
  for (const auto& [name, matrix] : entries) {
    if (ascii_iequals(value, name)) {
      return matrix;
    }
  }
  return std::nullopt;
}

struct OldMatrix {
  ParsedMatrix matrix;
  std::optional<ColorRange> range;
};

[[nodiscard]] std::optional<OldMatrix> parse_old_matrix(const std::string_view value) {
  if (ascii_iequals(value, "rec601")) {
    return OldMatrix{ParsedMatrix::st170_m, ColorRange::limited};
  }
  if (ascii_iequals(value, "pc.601") || ascii_iequals(value, "pc601")) {
    return OldMatrix{ParsedMatrix::st170_m, std::nullopt};
  }
  if (ascii_iequals(value, "rec709")) {
    return OldMatrix{ParsedMatrix::bt709, ColorRange::limited};
  }
  if (ascii_iequals(value, "pc.709") || ascii_iequals(value, "pc709")) {
    return OldMatrix{ParsedMatrix::bt709, std::nullopt};
  }
  if (ascii_iequals(value, "average")) {
    return OldMatrix{ParsedMatrix::average, std::nullopt};
  }
  if (ascii_iequals(value, "rec2020")) {
    return OldMatrix{ParsedMatrix::bt2020_ncl, ColorRange::limited};
  }
  if (ascii_iequals(value, "pc.2020") || ascii_iequals(value, "pc2020")) {
    return OldMatrix{ParsedMatrix::bt2020_ncl, std::nullopt};
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<RgbLumaMatrix> to_core_matrix(const ParsedMatrix matrix) {
  switch (matrix) {
    case ParsedMatrix::rgb:
      return RgbLumaMatrix::rgb;
    case ParsedMatrix::bt709:
      return RgbLumaMatrix::bt709;
    case ParsedMatrix::bt470_m:
      return RgbLumaMatrix::bt470_m;
    case ParsedMatrix::bt470_bg:
      return RgbLumaMatrix::bt470_bg;
    case ParsedMatrix::st170_m:
      return RgbLumaMatrix::st170_m;
    case ParsedMatrix::st240_m:
      return RgbLumaMatrix::st240_m;
    case ParsedMatrix::bt2020_ncl:
      return RgbLumaMatrix::bt2020_ncl;
    case ParsedMatrix::bt2020_cl:
      return RgbLumaMatrix::bt2020_cl;
    case ParsedMatrix::average:
      return RgbLumaMatrix::average;
    case ParsedMatrix::unspecified:
    case ParsedMatrix::ycgco:
    case ParsedMatrix::chroma_ncl:
    case ParsedMatrix::chroma_cl:
    case ParsedMatrix::ictcp:
      return std::nullopt;
  }
  return std::nullopt;
}

[[nodiscard]] MatrixSelection parse_matrix_selection(const char* const matrix_name, const AVSMap* const properties,
                                                     IScriptEnvironment* const env) {
  ColorRange source_range = ColorRange::full;
  ColorRange default_destination_range = ColorRange::full;
  if (properties != nullptr && env->propNumElements(properties, "_ColorRange") > 0) {
    const auto property_range =
        color_range_from_property(env->propGetIntSaturated(properties, "_ColorRange", 0, nullptr));
    if (!property_range.has_value()) {
      env->ThrowError("GreyScale: Unknown matrix.");
    }
    source_range = *property_range;
    default_destination_range = *property_range;
  }

  const std::string_view matrix_specifier = matrix_name == nullptr ? "" : matrix_name;
  const std::size_t first_colon = matrix_specifier.find(':');
  const std::size_t second_colon =
      first_colon == std::string_view::npos ? std::string_view::npos : matrix_specifier.find(':', first_colon + 1);
  if (second_colon != std::string_view::npos && second_colon + 1 != matrix_specifier.size()) {
    env->ThrowError("Invalid matrix specifier, too many parts");
  }
  const std::string_view matrix_part = matrix_specifier.substr(0, first_colon);
  const std::string_view range_part =
      first_colon == std::string_view::npos
          ? ""
          : matrix_specifier.substr(first_colon + 1, second_colon == std::string_view::npos
                                                         ? std::string_view::npos
                                                         : second_colon - first_colon - 1);

  ParsedMatrix matrix = ParsedMatrix::st170_m;
  ColorRange destination_range = default_destination_range;
  if (const auto old_matrix = parse_old_matrix(matrix_part); old_matrix.has_value()) {
    if (!range_part.empty() && !ascii_iequals(range_part, "auto")) {
      env->ThrowError("Error: this 'old-style' matrix string can only be followed by 'auto' color range");
    }
    matrix = old_matrix->matrix;
    destination_range = old_matrix->range.value_or(source_range);
  } else {
    if (!is_auto(matrix_part)) {
      const auto parsed_matrix = parse_new_matrix(matrix_part);
      if (!parsed_matrix.has_value()) {
        env->ThrowError("Convert: Unknown matrix");
      }
      matrix = *parsed_matrix == ParsedMatrix::unspecified ? ParsedMatrix::st170_m : *parsed_matrix;
    }

    if (ascii_iequals(range_part, "same")) {
      destination_range = source_range;
    } else if (!is_auto(range_part)) {
      const auto parsed_range = parse_range(range_part);
      if (!parsed_range.has_value()) {
        env->ThrowError("Convert: Unknown color range, must be 'auto', 'full', 'f', 'limited' or 'l'");
      }
      destination_range = *parsed_range;
    }
  }

  const auto core_matrix = to_core_matrix(matrix);
  if (!core_matrix.has_value()) {
    env->ThrowError("GreyScale: Unknown matrix.");
  }
  return {*core_matrix, source_range, destination_range};
}

} // namespace
Greyscale::Greyscale(PClip clip, const char* matrix, IScriptEnvironment* env)
    : GenericVideoFilter(clip), cpu_(allowed_cpu(env)) {
  if (matrix && !vi.IsRGB())
    env->ThrowError("Greyscale: matrix requires RGB");
  if (vi.IsRGB()) {
    auto f = child->GetFrame(0, env);
    auto s = parse_matrix_selection(matrix, env->getFramePropsRO(f), env);
    double kr = 0, kb = 0;
    switch (s.matrix) {
      case RgbLumaMatrix::rgb:
        break;
      case RgbLumaMatrix::bt709:
        kr = .2126;
        kb = .0722;
        break;
      case RgbLumaMatrix::bt470_m:
        kr = .3;
        kb = .11;
        break;
      case RgbLumaMatrix::bt470_bg:
      case RgbLumaMatrix::st170_m:
        kr = .299;
        kb = .114;
        break;
      case RgbLumaMatrix::st240_m:
        kr = .212;
        kb = .087;
        break;
      case RgbLumaMatrix::bt2020_ncl:
      case RgbLumaMatrix::bt2020_cl:
        kr = .2627;
        kb = .0593;
        break;
      case RgbLumaMatrix::average:
        kr = kb = 1.0 / 3;
        break;
    }
    out_range_ = s.destination_range == ColorRange::full ? 0 : 1;
    aif_greyscale_plan* p = nullptr;
    if (aif_greyscale_create(kr, kb, vi.BitsPerComponent(), s.source_range == ColorRange::full, !out_range_, cpu_, &p))
      env->ThrowError("Greyscale: cannot create matrix");
    plan_.reset(p);
  }
}
PVideoFrame __stdcall Greyscale::GetFrame(int n, IScriptEnvironment* env) {
  auto f = child->GetFrame(n, env);
  env->MakeWritable(&f);
  int status = 0;
  if (vi.IsRGB()) {
    uint8_t* data[3]{};
    int pitch[3]{};
    const int planes[] = {PLANAR_R, PLANAR_G, PLANAR_B};
    for (int i = 0; i < (vi.IsPlanar() ? 3 : 1); ++i) {
      int plane = vi.IsPlanar() ? planes[i] : 0;
      data[i] = f->GetWritePtr(plane);
      pitch[i] = f->GetPitch(plane);
    }
    status = aif_greyscale_rgb(plan_.get(), data, pitch, vi.width, vi.height, !vi.IsPlanar(), vi.NumComponents());
    env->propSetInt(env->getFramePropsRW(f), "_ColorRange", out_range_, PROPAPPENDMODE_REPLACE);
  } else if (vi.IsYUY2())
    status = aif_greyscale_yuy2(f->GetWritePtr(), f->GetPitch(), vi.width, vi.height, cpu_);
  else
    for (int plane : {PLANAR_U, PLANAR_V}) {
      status =
          aif_greyscale_chroma(f->GetWritePtr(plane), f->GetPitch(plane), f->GetRowSize(plane) / vi.ComponentSize(),
                               f->GetHeight(plane), vi.BitsPerComponent(), cpu_);
      if (status)
        break;
    }
  if (status)
    env->ThrowError("Greyscale: kernel failed (%d)", status);
  return f;
}
AVSValue __cdecl Greyscale::Create(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (clip->GetVideoInfo().NumComponents() == 1)
    return clip;
  return new Greyscale(clip, args[1].AsString(nullptr), env);
}
} // namespace aif::filters::greyscale
