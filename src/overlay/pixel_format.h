#pragma once
#include <avisynth.h>
#include <array>
#include <string_view>
namespace aif::filters::overlay {
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

[[nodiscard]] inline bool equals_ascii_ignore_case(const std::string_view left, const std::string_view right) noexcept {
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

[[nodiscard]] inline int pixel_type_from_name(const char* const name) noexcept {
  const std::string_view requested(name);
  for (const auto& candidate : kPixelTypeNames) {
    if (equals_ascii_ignore_case(requested, candidate.name)) {
      return candidate.pixel_type;
    }
  }
  return VideoInfo::CS_UNKNOWN;
}

} // namespace aif::filters::overlay
