#pragma once
#include <avisynth.h>
namespace aif::filters::histogram {
class TextFrame final : public IClip {
  VideoInfo vi_;
  PVideoFrame frame_;

public:
  TextFrame(const VideoInfo& vi, PVideoFrame frame) : vi_(vi), frame_(frame) {
    vi_.num_frames = 1;
    vi_.num_audio_samples = 0;
    vi_.audio_samples_per_second = 0;
  }
  const VideoInfo& __stdcall GetVideoInfo() override { return vi_; }
  PVideoFrame __stdcall GetFrame(int, IScriptEnvironment*) override { return frame_; }
  bool __stdcall GetParity(int) override { return false; }
  void __stdcall GetAudio(void*, int64_t, int64_t, IScriptEnvironment*) override {}
  int __stdcall SetCacheHints(int, int) override { return 0; }
};
} // namespace aif::filters::histogram
