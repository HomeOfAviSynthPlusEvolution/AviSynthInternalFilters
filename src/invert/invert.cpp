// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from AviSynthPlus avs_core/filters/layer.cpp.
#include "invert.h"
#include "kernel_adapter.h"
#include "invert/kernel.h"
#include <cctype>
#include <cstring>
namespace aif::filters::invert {
Invert::Invert(PClip clip, const char* channels, IScriptEnvironment* env) : GenericVideoFilter(clip) {
  for (; *channels; ++channels) {
    const char* names = "BGRAYUV";
    const char* p = std::strchr(names, std::toupper(static_cast<unsigned char>(*channels)));
    if (p)
      selected_[p - names] = true;
  }
  cpu_ = allowed_cpu(env);
}
PVideoFrame __stdcall Invert::GetFrame(int n, IScriptEnvironment* env) {
  auto frame = child->GetFrame(n, env);
  env->MakeWritable(&frame);
  int bits = vi.BitsPerComponent(), bytes = vi.ComponentSize();
  if (vi.IsPlanar()) {
    const bool rgb = vi.IsRGB();
    const int planes[] = {rgb ? PLANAR_B : PLANAR_Y, rgb ? PLANAR_G : PLANAR_U, rgb ? PLANAR_R : PLANAR_V, PLANAR_A};
    for (int c = 0; c < vi.NumComponents(); ++c) {
      int selection = c == 3 ? 3 : rgb ? c : c + 4;
      if (!selected_[selection])
        continue;
      uint32_t mask = bits == 32 ? 0 : (1u << bits) - 1;
      int mode = bits == 32 ? (!rgb && (c == 1 || c == 2) ? 0 : 1) : -1;
      int p = planes[c];
      if (aif_invert_apply(frame->GetWritePtr(p), frame->GetPitch(p), frame->GetRowSize(p), frame->GetHeight(p), &mask,
                           bytes, mode, cpu_))
        env->ThrowError("Invert: invalid plane");
    }
  } else {
    uint8_t mask[8]{};
    int count = vi.IsYUY2() ? 4 : vi.NumComponents();
    for (int c = 0; c < count; ++c) {
      int selection = vi.IsYUY2() ? (c % 2 == 0 ? 4 : c == 1 ? 5 : 6) : c;
      if (selected_[selection])
        for (int b = 0; b < bytes; ++b)
          mask[c * bytes + b] = 255;
    }
    if (aif_invert_apply(frame->GetWritePtr(), frame->GetPitch(), frame->GetRowSize(), frame->GetHeight(), mask,
                         count * bytes, -1, cpu_))
      env->ThrowError("Invert: invalid packed frame");
  }
  return frame;
}
AVSValue __cdecl Invert::Create(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  return new Invert(clip, args[1].AsString(clip->GetVideoInfo().IsRGB() ? "RGBA" : "YUVA"), env);
}
} // namespace aif::filters::invert
