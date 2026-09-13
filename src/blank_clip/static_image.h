// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
namespace aif::filters::blank_clip {
class StaticImage final : public IClip {
public:
  StaticImage(const VideoInfo& video_info, PVideoFrame frame, const bool parity);

  PVideoFrame __stdcall GetFrame(int, IScriptEnvironment*) override;

  void __stdcall GetAudio(void* const buffer, const int64_t, const int64_t count, IScriptEnvironment*) override;

  const VideoInfo& __stdcall GetVideoInfo() override;

  bool __stdcall GetParity(const int n) override;

  int __stdcall SetCacheHints(const int cachehints, const int) override;

private:
  const VideoInfo video_info_;
  const PVideoFrame frame_;
  const bool parity_;
};

} // namespace aif::filters::blank_clip
