// SPDX-License-Identifier: GPL-2.0-or-later
// Adapted from AviSynth layer.cpp.
#include "merge_rgb.h"
#include "kernel_adapter.h"
#include <string>
namespace aif::filters::rgb_merge {
MergeRGB::MergeRGB(PClip _child, PClip _blue, PClip _green, PClip _red, PClip _alpha, const char* pixel_type,
                   IScriptEnvironment* env)
    : GenericVideoFilter(_child), cpu_mask_(cpu(env)), blue(_blue), green(_green), red(_red), alpha(_alpha),
      viB(blue->GetVideoInfo()), viG(green->GetVideoInfo()), viR(red->GetVideoInfo()),
      viA(((alpha) ? alpha : child)->GetVideoInfo()), myname((alpha) ? "MergeARGB" : "MergeRGB") {
  vi = viR; // comparison base

  if ((vi.BitsPerComponent() != viB.BitsPerComponent()) || (vi.BitsPerComponent() != viG.BitsPerComponent()) ||
      (vi.BitsPerComponent() != viR.BitsPerComponent()) || (vi.BitsPerComponent() != viA.BitsPerComponent()))
    env->ThrowError("%s: All clips must have the same bit depth.", myname);

  if ((vi.width != viB.width) || (vi.width != viG.width) || (vi.width != viR.width) || (vi.width != viA.width))
    env->ThrowError("%s: All clips must have the same width.", myname);

  if ((vi.height != viB.height) || (vi.height != viG.height) || (vi.height != viR.height) || (vi.height != viA.height))
    env->ThrowError("%s: All clips must have the same height.", myname);

  const int is_any_planar_rgb = viR.IsPlanarRGB() || viR.IsPlanarRGBA() || viG.IsPlanarRGB() || viG.IsPlanarRGBA() ||
                                viB.IsPlanarRGB() || viB.IsPlanarRGBA() || viA.IsPlanarRGB() || viA.IsPlanarRGBA();

  const int bits_per_pixel = viR.BitsPerComponent();
  const bool empty_pixel_type = pixel_type == nullptr || *pixel_type == 0;

  // planar rgb target if
  // - pixel_type is "rgb" or
  // - pixel_type not specified and
  //   - bit depth is not 8 or 16 (cannot have packed rgb representation) or
  //   - any of the input clips is planar rgb

  if (!lstrcmpi(pixel_type, "rgb") ||
      (empty_pixel_type && (is_any_planar_rgb || (bits_per_pixel != 8 && bits_per_pixel != 16)))) {
    switch (bits_per_pixel) {
      case 8:
        vi.pixel_type = alpha ? VideoInfo::CS_RGBAP8 : VideoInfo::CS_RGBP8;
        break;
      case 10:
        vi.pixel_type = alpha ? VideoInfo::CS_RGBAP10 : VideoInfo::CS_RGBP10;
        break;
      case 12:
        vi.pixel_type = alpha ? VideoInfo::CS_RGBAP12 : VideoInfo::CS_RGBP12;
        break;
      case 14:
        vi.pixel_type = alpha ? VideoInfo::CS_RGBAP14 : VideoInfo::CS_RGBP14;
        break;
      case 16:
        vi.pixel_type = alpha ? VideoInfo::CS_RGBAP16 : VideoInfo::CS_RGBP16;
        break;
      case 32:
        vi.pixel_type = alpha ? VideoInfo::CS_RGBAPS : VideoInfo::CS_RGBPS;
        break;
    }
  } else if (empty_pixel_type && vi.BitsPerComponent() == 8) {
    // default for 8 bit
    vi.pixel_type = VideoInfo::CS_BGR32;
  } else if (empty_pixel_type && vi.BitsPerComponent() == 16) {
    // default for 16 bit
    vi.pixel_type = VideoInfo::CS_BGR64;
  } else {
    // explicitely given output pixel type

    // first try
    // Append bit depth and check
    std::string format = pixel_type;
    // RGBP --> RGBPS, Y -> Y16, YUV420 -> YUV420P10
    if (!lstrcmpi(pixel_type, "y")) {
      format = format + std::to_string(bits_per_pixel); // Y8..Y16, also Y32
    } else if (!lstrcmpi(pixel_type, "rgbp") || !lstrcmpi(pixel_type, "rgbap")) {
      if (bits_per_pixel == 32)
        format = format + "S"; // RGBAPS
      else
        format = format + std::to_string(bits_per_pixel); // RGBP16
    } else {
      // hopefully like "yuv420" or "yuva444"
      if (bits_per_pixel == 32)
        format = format + "PS"; // YUV420PS
      else
        format = format + "P" + std::to_string(bits_per_pixel); // YUV420P16
    }

    int new_pixel_type = pixel_type_id(format.c_str(), env);
    if (new_pixel_type == VideoInfo::CS_UNKNOWN) {
      new_pixel_type = pixel_type_id(pixel_type, env);
      if (new_pixel_type == VideoInfo::CS_UNKNOWN)
        env->ThrowError("%s: invalid pixel_type!", myname);
    }
    // new output format
    vi.pixel_type = new_pixel_type;
  }

  if (vi.BitsPerComponent() != bits_per_pixel)
    env->ThrowError("%s: target bit depth (%d) must match with sources (%d)", myname, vi.BitsPerComponent(),
                    bits_per_pixel);

  if (!vi.IsRGB())
    env->ThrowError("%s: target format must be an RGB format", myname);

  if (alpha && vi.NumComponents() != 4)
    env->ThrowError("MergeARGB: target format must have an alpha channel");

  // When not in ARGB mode, target is still allowed to have an alpha channel.
  // If no alpha source is given, target alpha will be filled by default 0 value.

  if (alpha && (viA.IsRGB24() || viA.IsRGB48() || viA.IsPlanarRGB()))
    env->ThrowError("MergeARGB: Alpha source channel cannot be obtained from RGB24, RGB48 or alphaless planar RGB");
}

PVideoFrame MergeRGB::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame frames[] = {red->GetFrame(n, env), green->GetFrame(n, env), blue->GetFrame(n, env),
                          alpha ? alpha->GetFrame(n, env) : PVideoFrame()};
  const VideoInfo* infos[] = {&viR, &viG, &viB, &viA};
  const int planes[] = {PLANAR_R, PLANAR_G, PLANAR_B, PLANAR_A};
  PVideoFrame dst = env->NewVideoFrameP(vi, &frames[0]);
  if (!viR.IsRGB()) {
    auto props = env->getFramePropsRW(dst);
    env->propDeleteKey(props, "_Matrix");
    env->propDeleteKey(props, "_ChromaLocation");
  }
  const uint8_t* src[4] = {};
  int sp[4] = {}, layout[4] = {};
  uint8_t* out[4] = {};
  int dp[4] = {};
  for (int c = 0; c < 4; ++c) {
    if (c < 3 || alpha) {
      const auto& info = *infos[c];
      int plane = info.IsPlanar() ? (info.IsRGB() ? planes[c] : PLANAR_Y) : 0;
      src[c] = frames[c]->GetReadPtr(plane);
      sp[c] = frames[c]->GetPitch(plane);
      layout[c] = info.IsPlanar() ? 1 : info.IsYUY2() ? 2 : info.NumComponents();
    }
    if (vi.IsPlanar() && c < vi.NumComponents()) {
      out[c] = dst->GetWritePtr(planes[c]);
      dp[c] = dst->GetPitch(planes[c]);
    }
  }
  if (!vi.IsPlanar()) {
    out[0] = dst->GetWritePtr();
    dp[0] = dst->GetPitch();
  }
  if (aif_rgb_merge_render(src, sp, layout, out, dp, vi.width, vi.height, vi.ComponentSize(),
                           vi.IsPlanar() ? 1 : vi.NumComponents(), cpu_mask_))
    env->ThrowError("%s: layout failed", myname);
  return dst;
}
AVSValue MergeRGB::Create(AVSValue args, void* mode, IScriptEnvironment* env) {
  if (mode) // ARGB
    return new MergeRGB(args[0].AsClip(), args[3].AsClip(), args[2].AsClip(), args[1].AsClip(), args[0].AsClip(),
                        args[4].AsString(""), env);
  else // RGB[type]
    return new MergeRGB(args[0].AsClip(), args[2].AsClip(), args[1].AsClip(), args[0].AsClip(), 0, args[3].AsString(""),
                        env);
}

} // namespace aif::filters::rgb_merge
