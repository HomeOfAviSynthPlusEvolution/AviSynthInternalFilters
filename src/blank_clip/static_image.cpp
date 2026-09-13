// SPDX-License-Identifier: GPL-2.0-or-later
#include "static_image.h"
#include <utility>
#include <cstring>
namespace aif::filters::blank_clip {
StaticImage::StaticImage(const VideoInfo& video_info, PVideoFrame frame, const bool parity)
    : video_info_(video_info), frame_(std::move(frame)), parity_(parity) {
}
PVideoFrame __stdcall StaticImage::GetFrame(int, IScriptEnvironment*) {
  return frame_;
}
void __stdcall StaticImage::GetAudio(void* const buffer, const int64_t, const int64_t count, IScriptEnvironment*) {
  std::memset(buffer, 0, static_cast<std::size_t>(video_info_.BytesFromAudioSamples(count)));
}
const VideoInfo& __stdcall StaticImage::GetVideoInfo() {
  return video_info_;
}
bool __stdcall StaticImage::GetParity(const int n) {
  return (video_info_.IsFieldBased() ? (n & 1) != 0 : false) != parity_;
}
int __stdcall StaticImage::SetCacheHints(const int cachehints, const int) {
  switch (cachehints) {
    case CACHE_DONT_CACHE_ME:
      return 1;
    case CACHE_GET_MTMODE:
      return MT_NICE_FILTER;
    case CACHE_GET_DEV_TYPE:
      return DEV_TYPE_CPU;
    case CACHE_GET_CHILD_DEV_TYPE:
      return DEV_TYPE_ANY;
    default:
      return 0;
  }
}
} // namespace aif::filters::blank_clip
