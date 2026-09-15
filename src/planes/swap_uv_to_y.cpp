// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from AviSynthPlus avs_core/filters/planeswap.cpp.
#include "swap_uv_to_y.h"
#include "kernel_adapter.h"
namespace aif::filters::planes {
AVSValue __cdecl SwapUVToY::CreateUToY(AVSValue args, void*, IScriptEnvironment* env) {
  return new SwapUVToY(args[0].AsClip(), UToY, env);
}

AVSValue __cdecl SwapUVToY::CreateUToY8(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  return new SwapUVToY(clip, (clip->GetVideoInfo().IsYUY2()) ? YUY2UToY8 : UToY8, env);
}

AVSValue __cdecl SwapUVToY::CreateYToY8(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  if (clip->GetVideoInfo().IsYUY2())
    return convert(clip, "ConvertToY8", env);
  else
    return new SwapUVToY(clip, YToY8, env);
}

AVSValue __cdecl SwapUVToY::CreateVToY(AVSValue args, void*, IScriptEnvironment* env) {
  return new SwapUVToY(args[0].AsClip(), VToY, env);
}

AVSValue __cdecl SwapUVToY::CreateVToY8(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  return new SwapUVToY(clip, (clip->GetVideoInfo().IsYUY2()) ? YUY2VToY8 : VToY8, env);
}

AVSValue __cdecl SwapUVToY::CreateAnyToY8(AVSValue args, void* user_data, IScriptEnvironment* env) {
  int mode = (int)(intptr_t)(user_data);
  PClip clip = args[0].AsClip();
  const VideoInfo& vi_input = clip->GetVideoInfo();

  // 161205: Packed RGB PlaneToY("R"),g,b,a or ExtractR,G,B,A
  // A generic way for using these PlaneToY() or Extract... functions for packed RGB types
  // We convert them to planar RGB (R,G,B plane reqest) or planar RGBA (only if A plane requested)
  if (vi_input.IsRGB() && !vi_input.IsPlanarRGB() && !vi_input.IsPlanarRGBA()) {
    if (mode == AToY8 || mode == RToY8 || mode == GToY8 || mode == BToY8) {
      clip = convert(clip, mode == AToY8 ? "ConvertToPlanarRGBA" : "ConvertToPlanarRGB", env);
    }
  }

  if (clip->GetVideoInfo().IsYUY2() && mode == YToY8)
    return convert(clip, "ConvertToY8", env);

  if (clip->GetVideoInfo().IsY() && mode == YToY8)
    return clip;

  return new SwapUVToY(clip, mode, env);
}

AVSValue __cdecl SwapUVToY::CreatePlaneToY8(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();

  const VideoInfo& vi_input = clip->GetVideoInfo();

  const char* plane = args[1].AsString("");
  int mode = 0;
  // enum {UToY=1, VToY, UToY8, VToY8, YUY2UToY8, YUY2VToY8, AToY8, RToY8, GToY8, BToY8, YToY8};
  if (!lstrcmpi(plane, "Y"))
    mode = YToY8;
  else if (!lstrcmpi(plane, "U"))
    mode = vi_input.IsYUY2() ? YUY2UToY8 : UToY8;
  else if (!lstrcmpi(plane, "V"))
    mode = vi_input.IsYUY2() ? YUY2VToY8 : VToY8;
  else if (!lstrcmpi(plane, "A"))
    mode = AToY8;
  else if (!lstrcmpi(plane, "R"))
    mode = RToY8;
  else if (!lstrcmpi(plane, "G"))
    mode = GToY8;
  else if (!lstrcmpi(plane, "B"))
    mode = BToY8;
  else
    env->ThrowError("PlaneToY: Invalid plane!");

  return CreateAnyToY8(args, (void*)(intptr_t)mode, env);
}

SwapUVToY::SwapUVToY(PClip _child, int _mode, IScriptEnvironment* env)
    : GenericVideoFilter(_child), cpu_mask_(cpu(env)), mode(_mode) {
  bool YUVmode = mode == YToY8 || mode == UToY8 || mode == VToY8 || mode == UToY || mode == VToY || mode == YUY2UToY8 ||
                 mode == YUY2VToY8;
  bool RGBmode = mode == RToY8 || mode == GToY8 || mode == BToY8;
  bool Alphamode = mode == AToY8;

  if (!vi.IsYUVA() && !vi.IsPlanarRGBA() && Alphamode)
    env->ThrowError("PlaneToY: Clip has no Alpha channel!");

  if (!vi.IsYUV() && !vi.IsYUVA() && YUVmode)
    env->ThrowError("PlaneToY: clip is not YUV!");

  if (!vi.IsPlanarRGB() && !vi.IsPlanarRGBA() && RGBmode)
    env->ThrowError("PlaneToY: clip is not planar RGB!");

  if (vi.NumComponents() == 1 && mode != YToY8)
    env->ThrowError("PlaneToY: channel cannot be extracted from a greyscale clip!");

  if (YUVmode && (mode != YToY8)) {
    vi.height >>= vi.GetPlaneHeightSubsampling(PLANAR_U);
    vi.width >>= vi.GetPlaneWidthSubsampling(PLANAR_U);
  }

  if (mode == YToY8 || mode == UToY8 || mode == VToY8 || mode == YUY2UToY8 || mode == YUY2VToY8 || RGBmode ||
      Alphamode) {
    switch (vi.BitsPerComponent()) // although name is Y8, it means that greyscale stays in the same bitdepth
    {
      case 8:
        vi.pixel_type = VideoInfo::CS_Y8;
        break;
      case 10:
        vi.pixel_type = VideoInfo::CS_Y10;
        break;
      case 12:
        vi.pixel_type = VideoInfo::CS_Y12;
        break;
      case 14:
        vi.pixel_type = VideoInfo::CS_Y14;
        break;
      case 16:
        vi.pixel_type = VideoInfo::CS_Y16;
        break;
      case 32:
        vi.pixel_type = VideoInfo::CS_Y32;
        break;
    }
  }
}

PVideoFrame __stdcall SwapUVToY::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);

  bool NonYUY2toY8 = true;
  int target_plane, source_plane;
  switch (mode) {
    case YToY8:
      source_plane = PLANAR_Y;
      target_plane = PLANAR_Y;
      break;
    case UToY8:
      source_plane = PLANAR_U;
      target_plane = PLANAR_Y;
      break;
    case VToY8:
      source_plane = PLANAR_V;
      target_plane = PLANAR_Y;
      break;
    case RToY8:
      source_plane = PLANAR_R;
      target_plane = PLANAR_G;
      break; // Planar RGB: GBR!
    case GToY8:
      source_plane = PLANAR_G;
      target_plane = PLANAR_G;
      break;
    case BToY8:
      source_plane = PLANAR_B;
      target_plane = PLANAR_G;
      break;
    case AToY8:
      source_plane = PLANAR_A;
      target_plane = vi.IsYUVA() ? PLANAR_Y : PLANAR_G;
      break; // Planar RGB: GBR!
    default:
      NonYUY2toY8 = false;
  }
  if (NonYUY2toY8) {
    // !! if offsets would be size_t, be cautious when you subtract two unsigned size_t variables
    const int offset =
        src->GetOffset(source_plane) - src->GetOffset(target_plane); // very naughty - don't do this at home!!
                                                                     // Abuse Subframe to snatch the U/V/R/G/B/A plane
    PVideoFrame sub = env->Subframe(src, offset, src->GetPitch(source_plane), src->GetRowSize(source_plane),
                                    src->GetHeight(source_plane));
    // We have a single plane. It's safe to mod props after a subframe.
    // Remove props that are irrelevant to a single plane.
    // _ChromaLocation, (_Primaries, _Transfer)
    auto props = env->getFramePropsRW(sub);
    env->propDeleteKey(props, "_ChromaLocation");
    // keep _Matrix (?) fixme: really?
    if (mode == AToY8) // alpha is always full range, otherwise keep source
      env->propSetInt(props, "_ColorRange", 0, AVSPropAppendMode::PROPAPPENDMODE_REPLACE);
    // else we keep _ColorRange value (if any)
    return sub;
  }

  PVideoFrame dst = env->NewVideoFrameP(vi, &src);
  if (mode == YUY2UToY8 || mode == YUY2VToY8 || vi.IsYUY2()) {
    const BYTE* srcp = src->GetReadPtr();
    BYTE* dstp = dst->GetWritePtr();
    int src_pitch = src->GetPitch();
    int dst_pitch = dst->GetPitch();
    int pos = (mode == YUY2UToY8 || mode == UToY) ? 1 : 3; // YUYV U=offset#1 V=offset#3

    if (!vi.IsYUY2())
      env->propDeleteKey(env->getFramePropsRW(dst), "_ChromaLocation");
    if (aif_planes_extract_uv(srcp, src_pitch, dstp, dst_pitch, vi.width, vi.height, pos == 3, vi.IsYUY2(), cpu_mask_))
      env->ThrowError("PlaneToY: invalid YUY2 frame");
    return dst;
  }

  // Planar to Planar. Only two modes possible UToY and VToY
  // Copy U or V to Y and set the other chroma planes to grey
  const int plane = mode == UToY ? PLANAR_U : PLANAR_V;
  env->BitBlt(dst->GetWritePtr(PLANAR_Y), dst->GetPitch(PLANAR_Y), src->GetReadPtr(plane), src->GetPitch(plane),
              src->GetRowSize(plane), src->GetHeight(plane));

  // Clear chroma
  int pitch = dst->GetPitch(PLANAR_U);
  int height = dst->GetHeight(PLANAR_U);
  int rowsize = dst->GetRowSize(PLANAR_U);
  BYTE* dstp_u = dst->GetWritePtr(PLANAR_U);
  BYTE* dstp_v = dst->GetWritePtr(PLANAR_V);

  if (vi.ComponentSize() == 1) { // 8bit
    fill_chroma<BYTE>(dstp_u, dstp_v, height, rowsize, pitch, 0x80, env, cpu_mask_);
  } else if (vi.ComponentSize() == 2) {                   // 16bit
    uint16_t grey_val = 1 << (vi.BitsPerComponent() - 1); // 0x8000 for 16 bit
    fill_chroma<uint16_t>(dstp_u, dstp_v, height, rowsize, pitch, grey_val, env, cpu_mask_);
  } else { // 32bit(float)
    float grey_val = uv8tof(128);
    fill_chroma<float>(dstp_u, dstp_v, height, rowsize, pitch, grey_val, env, cpu_mask_);
  }

  return dst;
}

} // namespace aif::filters::planes
