// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from AviSynthPlus avs_core/filters/limiter.cpp.
#include "limiter.h"
#include "kernel_adapter.h"
#include <cmath>
#include <limits>
#include <string>
#include <cctype>
namespace aif::filters::limiter {
Limiter::Limiter(PClip clip, float yl, float yh, float cl, float ch, int show, bool scale, IScriptEnvironment* env)
    : GenericVideoFilter(clip), show_(show) {
  if (!vi.IsYUV() && !vi.IsYUVA())
    env->ThrowError("Limiter: Source must be YUV or YUVA");
  if (show && !vi.IsYUY2() && !vi.Is444() && !vi.Is420())
    env->ThrowError("Limiter: show requires 444, 420 or YUY2");
  limits_.bits = vi.BitsPerComponent();
  bool fp = limits_.bits == 32;
  int factor = fp ? 1 : 1 << (limits_.bits - 8);
  float inputs[] = {yl, yh, cl, ch}, values[4];
  for (int i = 0; i < 4; ++i) {
    float v = inputs[i];
    if (!std::isfinite(v))
      env->ThrowError("Limiter: limits must be finite");
    if (v == -9999.0f) {
      v = float(i % 2 == 0 ? 16 : i == 1 ? 235 : 240);
      if (fp)
        v = (i >= 2 ? v - 128 : v) / 255.0f;
      else
        v *= factor;
    } else if (fp) {
      if (scale)
        v = (i >= 2 ? v - 128 : v) / 255.0f;
    } else {
      if (scale)
        v = v * float(factor) + 0.5f;
      if (!std::isfinite(v) || double(v) < double(std::numeric_limits<int>::min()) ||
          double(v) > double(std::numeric_limits<int>::max()))
        env->ThrowError("Limiter: invalid limit");
      v = float(int(v));
    }
    if (!std::isfinite(v) || (!fp && (v < 0 || v > float((1u << limits_.bits) - 1))))
      env->ThrowError("Limiter: invalid limit");
    values[i] = v;
  }
  limits_.min_luma = values[0];
  limits_.max_luma = values[1];
  limits_.min_chroma = values[2];
  limits_.max_chroma = values[3];
  if (values[0] > values[1] || values[2] > values[3])
    env->ThrowError("Limiter: minimum exceeds maximum");
  cpu_ = allowed_cpu(env);
}
PVideoFrame __stdcall Limiter::GetFrame(int n, IScriptEnvironment* env) {
  auto f = child->GetFrame(n, env);
  env->MakeWritable(&f);
  int status = 0;
  if (show_) {
    uint8_t* data[] = {f->GetWritePtr(), nullptr, nullptr};
    int pitch[] = {f->GetPitch(), 0, 0};
    if (!vi.IsYUY2()) {
      data[1] = f->GetWritePtr(PLANAR_U);
      data[2] = f->GetWritePtr(PLANAR_V);
      pitch[1] = f->GetPitch(PLANAR_U);
      pitch[2] = f->GetPitch(PLANAR_V);
    }
    status = aif_limiter_show(data, pitch, vi.width, vi.height, &limits_, vi.IsYUY2() ? 2 : vi.Is420() ? 1 : 0, show_);
  } else if (vi.IsYUY2())
    status = aif_limiter_apply(f->GetWritePtr(), f->GetPitch(), vi.width, vi.height, &limits_, 0, 1, cpu_);
  else {
    const int planes[] = {PLANAR_Y, PLANAR_U, PLANAR_V};
    for (int i = 0; i < (vi.IsY() ? 1 : 3); ++i) {
      int p = planes[i];
      status = aif_limiter_apply(f->GetWritePtr(p), f->GetPitch(p), f->GetRowSize(p) / vi.ComponentSize(),
                                 f->GetHeight(p), &limits_, i != 0, 0, cpu_);
      if (status)
        break;
    }
  }
  if (status)
    env->ThrowError("Limiter: invalid kernel input");
  return f;
}
AVSValue __cdecl Limiter::Create(AVSValue args, void*, IScriptEnvironment* env) {
  int show = 0;
  if (args[5].Defined()) {
    std::string s = args[5].AsString();
    for (char& c : s)
      c = char(std::tolower(static_cast<unsigned char>(c)));
    if (s == "luma")
      show = 1;
    else if (s == "luma_grey")
      show = 2;
    else if (s == "chroma")
      show = 3;
    else if (s == "chroma_grey")
      show = 4;
    else
      env->ThrowError("Limiter: invalid show mode");
  }
  return new Limiter(args[0].AsClip(), args[1].AsFloatf(-9999), args[2].AsFloatf(-9999), args[3].AsFloatf(-9999),
                     args[4].AsFloatf(-9999), show, args[6].AsBool(false), env);
}
} // namespace aif::filters::limiter
