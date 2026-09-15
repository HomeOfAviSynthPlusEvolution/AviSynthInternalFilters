// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include <vector>

// Never pass frames allocated by one environment to another environment's
// Subframe/MakeWritable/cache machinery. The reference owns independent frames.
class ReferenceClip final : public IClip {
  PClip source_;
  IScriptEnvironment* source_env_;
  VideoInfo vi_;
  std::vector<PVideoFrame> frames_;

public:
  ReferenceClip(PClip source, IScriptEnvironment* source_env, IScriptEnvironment* env)
      : source_(source), source_env_(source_env), vi_(source->GetVideoInfo()) {
    for (int n = 0; n < vi_.num_frames; ++n) {
      auto src = source_->GetFrame(n, source_env_);
      auto dst = env->NewVideoFrame(vi_);
      const int yuv[] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
      const int rgb[] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
      const int* planes = vi_.IsRGB() ? rgb : yuv;
      for (int c = 0; c < (vi_.IsPlanar() ? vi_.NumComponents() : 1); ++c) {
        int p = vi_.IsPlanar() ? planes[c] : 0;
        env->BitBlt(dst->GetWritePtr(p), dst->GetPitch(p), src->GetReadPtr(p), src->GetPitch(p), src->GetRowSize(p),
                    src->GetHeight(p));
      }
      env->copyFrameProps(src, dst);
      frames_.push_back(dst);
    }
  }
  const VideoInfo& __stdcall GetVideoInfo() override { return vi_; }
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment*) override { return frames_.at(n); }
  bool __stdcall GetParity(int n) override { return source_->GetParity(n); }
  void __stdcall GetAudio(void* buffer, int64_t start, int64_t count, IScriptEnvironment*) override {
    source_->GetAudio(buffer, start, count, source_env_);
  }
  int __stdcall SetCacheHints(int, int) override { return 0; }
};
inline std::vector<AVSValue> reference_arguments(const std::vector<AVSValue>& args, IScriptEnvironment* source_env,
                                                 IScriptEnvironment* env) {
  auto result = args;
  for (auto& arg : result)
    if (arg.IsClip())
      arg = PClip(new ReferenceClip(arg.AsClip(), source_env, env));
  return result;
}
