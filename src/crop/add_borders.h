// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2002 Ben Rudiak-Gould et al.
// Copyright (C) 2026 AviSynthPlus-IF contributors
// Derived from AviSynthPlus avs_core/filters/transform.cpp.

#pragma once

#include <avisynth.h>
#include <cstdint>

namespace aif::filters::crop {
class AddBorders final : public GenericVideoFilter {
public:
  AddBorders(const int left, const int top, const int right, const int bottom, const int color, const bool color_is_yuv,
             PClip child, IScriptEnvironment* env);

  PVideoFrame __stdcall GetFrame(const int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(const int cachehints, const int) override;

private:
  void validate_yuv_alignment(IScriptEnvironment* env) const;

  void add_planar_borders(const PVideoFrame& source, PVideoFrame& destination, IScriptEnvironment* env) const;

  void add_packed_borders(const PVideoFrame& source, PVideoFrame& destination, IScriptEnvironment* env) const;

  int left_{};
  int top_{};
  int right_{};
  int bottom_{};
  std::uint32_t color_{};
  int xsub_{};
  int ysub_{};
  bool color_is_yuv_{};
  bool is_yuv_{};
  bool is_planar_rgb_{};
};

[[nodiscard]] PClip make_add_borders(PClip clip, int left, int top, int right, int bottom, int color, bool color_is_yuv,
                                     IScriptEnvironment* env);
[[nodiscard]] PClip make_add_borders_with_transient(PClip clip, int left, int top, int right, int bottom, int color,
                                                    bool color_is_yuv, const AVSValue& resample, const AVSValue& param1,
                                                    const AVSValue& param2, const AVSValue& param3,
                                                    const AVSValue& radius, IScriptEnvironment* env);
AVSValue __cdecl create_add_borders(AVSValue, void*, IScriptEnvironment*);

} // namespace aif::filters::crop
