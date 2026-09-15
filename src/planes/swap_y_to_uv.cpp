// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from AviSynthPlus avs_core/filters/planeswap.cpp.
#include "swap_y_to_uv.h"
#include "kernel_adapter.h"
namespace aif::filters::planes {
AVSValue __cdecl SwapYToUV::CreateYToUV(AVSValue args, void*, IScriptEnvironment* env) {
  return new SwapYToUV(args[0].AsClip(), args[1].AsClip(), NULL, NULL, env);
}

AVSValue __cdecl SwapYToUV::CreateYToYUV(AVSValue args, void*, IScriptEnvironment* env) {
  return new SwapYToUV(args[0].AsClip(), args[1].AsClip(), args[2].AsClip(), NULL, env);
}

AVSValue __cdecl SwapYToUV::CreateYToYUVA(AVSValue args, void*, IScriptEnvironment* env) {
  return new SwapYToUV(args[0].AsClip(), args[1].AsClip(), args[2].AsClip(), args[3].AsClip(), env);
}

SwapYToUV::SwapYToUV(PClip _child, PClip _clip, PClip _clipY, PClip _clipA, IScriptEnvironment* env)
    : GenericVideoFilter(_child), cpu_mask_(cpu(env)), clip(_clip), clipY(_clipY), clipA(_clipA) {
  if (!(vi.IsYUVA() || vi.IsY()) && clipA)
    env->ThrowError("YToUV: Only Y or YUVA data accepted when alpha clip is provided"); // Y, YUV and YUY2
  if (!vi.IsYUV() && !vi.IsYUVA()) {
    env->ThrowError("YToUV: Only YUV or YUVA data accepted"); // Y, YUV and YUY2
  }

  const VideoInfo& vi2 = clip->GetVideoInfo();
  if (!vi2.IsYUV() && !vi2.IsYUVA())
    env->ThrowError("YToUV: Only YUV or YUVA data accepted for U and V clips");
  if (vi.BitsPerComponent() != vi2.BitsPerComponent() || vi.ComponentSize() != vi2.ComponentSize())
    env->ThrowError("YToUV: U and V clips must have the same component format");
  if (vi.height != vi2.height)
    env->ThrowError("YToUV: Clips do not have the same height (U & V mismatch) !");
  if (vi.width != vi2.width)
    env->ThrowError("YToUV: Clips do not have the same width (U & V mismatch) !");
  if (vi.IsYUY2() != vi2.IsYUY2())
    env->ThrowError("YToUV: YUY2 Clips must have same colorspace (U & V mismatch) !");

  // no third parameter: no Y clip
  if (!clipY) {
    if (vi.IsYUY2())
      vi.width *= 2;
    else if (vi.IsY()) {
      switch (vi.BitsPerComponent()) {
        case 8:
          vi.pixel_type = VideoInfo::CS_YV24;
          break;
        case 10:
          vi.pixel_type = VideoInfo::CS_YUV444P10;
          break;
        case 12:
          vi.pixel_type = VideoInfo::CS_YUV444P12;
          break;
        case 14:
          vi.pixel_type = VideoInfo::CS_YUV444P14;
          break;
        case 16:
          vi.pixel_type = VideoInfo::CS_YUV444P16;
          break;
        case 32:
          vi.pixel_type = VideoInfo::CS_YUV444PS;
          break;
      }
    } else {
      vi.height <<= vi.GetPlaneHeightSubsampling(PLANAR_U);
      vi.width <<= vi.GetPlaneWidthSubsampling(PLANAR_U);
    }
    return;
  }

  // Y clip parameter exists, Y channel will be copied from that
  const VideoInfo& vi3 = clipY->GetVideoInfo();
  if (!vi3.IsYUV() && !vi3.IsYUVA())
    env->ThrowError("YToUV: Only YUV or YUVA data accepted for Y clip");
  if (vi.BitsPerComponent() != vi3.BitsPerComponent() || vi.ComponentSize() != vi3.ComponentSize())
    env->ThrowError("YToUV: Y and U/V clips must have the same component format");
  if (vi.IsYUY2() != vi3.IsYUY2())
    env->ThrowError("YToUV: YUY2 Clips must have same colorspace (UV & Y mismatch) !");

  if (vi.IsYUY2()) {
    if (vi3.height != vi.height)
      env->ThrowError("YToUV: Y clip does not have the same height of the UV clips! (YUY2 mode)");
    vi.width *= 2;
    if (vi3.width != vi.width)
      env->ThrowError("YToUV: Y clip does not have the double width of the UV clips!");
    return;
  }

  if (clipA) {
    if (vi.IsYUY2())
      env->ThrowError("YToUV: YUY2 not supported with alpha clip");
    const VideoInfo& vi4 = clipA->GetVideoInfo();
    if (!vi4.IsYUV() && !vi4.IsYUVA() && !vi4.IsPlanarRGBA())
      env->ThrowError("YToUV: Only YUV, YUVA or planar RGBA data accepted for alpha clip");
    if (vi4.width != vi3.width || vi4.height != vi3.height) // Y width == A width
      env->ThrowError("YToUV: different Y and A clip dimensions");
    if (vi4.BitsPerComponent() != vi3.BitsPerComponent() || vi4.ComponentSize() != vi3.ComponentSize())
      env->ThrowError("YToUV: different Y and A clip component format");
  }

  // Autogenerate destination colorformat
  switch (vi.BitsPerComponent()) { // CS_Sub_Width_2 and CS_Sub_Height_2 are 0, vi bitfield can or'd if change needed
    case 8:
      vi.pixel_type = clipA ? VideoInfo::CS_YUVA420 : vi.pixel_type = VideoInfo::CS_YV12;
      break;
    case 10:
      vi.pixel_type = clipA ? VideoInfo::CS_YUVA420P10 : vi.pixel_type = VideoInfo::CS_YUV420P10;
      break;
    case 12:
      vi.pixel_type = clipA ? VideoInfo::CS_YUVA420P12 : vi.pixel_type = VideoInfo::CS_YUV420P12;
      break;
    case 14:
      vi.pixel_type = clipA ? VideoInfo::CS_YUVA420P14 : VideoInfo::CS_YUV420P14;
      break;
    case 16:
      vi.pixel_type = clipA ? VideoInfo::CS_YUVA420P16 : VideoInfo::CS_YUV420P16;
      break;
    case 32:
      vi.pixel_type = clipA ? VideoInfo::CS_YUVA420PS : VideoInfo::CS_YUV420PS;
      break;
  }

  if (vi3.width == vi.width) // Y width == U width -> subsampling 1:1
    vi.pixel_type |= VideoInfo::CS_Sub_Width_1;
  else if (vi3.width == vi.width * 2)   // Y width == U width*2 -> horiz. subsampling 2
    vi.width *= 2;                      // YV12 subsampling CS_Sub_Width_2 is o.k.
  else if (vi3.width == vi.width * 4) { // Y width == U width*4 -> horiz. subsampling 4
    vi.pixel_type |= VideoInfo::CS_Sub_Width_4;
    vi.width *= 4; // final clip width is 3x of the U channel width
  } else
    env->ThrowError("YToUV: Video width ratio does not match any internal colorspace.");

  if (vi3.height == vi.height)
    vi.pixel_type |= VideoInfo::CS_Sub_Height_1;
  else if (vi3.height == vi.height * 2)
    vi.height *= 2;
  else if (vi3.height == vi.height * 4) {
    vi.pixel_type |= VideoInfo::CS_Sub_Height_4;
    vi.height *= 4;
  } else
    env->ThrowError("YToUV: Video height ratio does not match any internal colorspace.");
}

PVideoFrame __stdcall SwapYToUV::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrameP(vi, &src);

  if (vi.IsYUY2()) {
    const BYTE* srcp_u = src->GetReadPtr();
    const int pitch_u = src->GetPitch();

    PVideoFrame srcv = clip->GetFrame(n, env);
    const BYTE* srcp_v = srcv->GetReadPtr();
    const int pitch_v = srcv->GetPitch();

    BYTE* dstp = dst->GetWritePtr();
    const int rowsize = dst->GetRowSize();
    const int dst_pitch = dst->GetPitch();

    if (clipY) {
      PVideoFrame srcy = clipY->GetFrame(n, env);
      const BYTE* srcp_y = srcy->GetReadPtr();
      const int pitch_y = srcy->GetPitch();
      if (aif_planes_assemble(srcp_y, pitch_y, srcp_u, pitch_u, srcp_v, pitch_v, dstp, dst_pitch, rowsize / 2,
                              vi.height, cpu_mask_))
        env->ThrowError("YToUV: invalid YUY2 frame");
    } else if (aif_planes_assemble(nullptr, 0, srcp_u, pitch_u, srcp_v, pitch_v, dstp, dst_pitch, rowsize / 2,
                                   vi.height, cpu_mask_))
      env->ThrowError("YToUV: invalid YUY2 frame");

    return dst;
  }

  // Planar:
  env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_Y), src->GetPitch(PLANAR_Y),
              src->GetRowSize(PLANAR_Y), src->GetHeight(PLANAR_Y));

  src = clip->GetFrame(n, env);
  env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_Y), src->GetPitch(PLANAR_Y),
              src->GetRowSize(PLANAR_Y), src->GetHeight(PLANAR_Y));

  if (clipA) {
    int source_plane = (clipA->GetVideoInfo().IsPlanarRGBA() || clipA->GetVideoInfo().IsYUVA()) ? PLANAR_A : PLANAR_Y;
    src = clipA->GetFrame(n, env);
    env->BitBlt(dst->GetWritePtr(PLANAR_A), dst->GetPitch(PLANAR_A), src->GetReadPtr(source_plane),
                src->GetPitch(source_plane), src->GetRowSize(source_plane), src->GetHeight(source_plane));
  }

  if (clipY) {
    src = clipY->GetFrame(n, env);
    env->BitBlt(dst->GetWritePtr(PLANAR_Y), dst->GetPitch(PLANAR_Y), src->GetReadPtr(PLANAR_Y), src->GetPitch(PLANAR_Y),
                src->GetRowSize(PLANAR_Y), src->GetHeight(PLANAR_Y));
    return dst;
  }

  // if no Y script was given, fill Y plane with neutral value
  // Luma = 126 (0x7e)
  BYTE* dstp = dst->GetWritePtr(PLANAR_Y);
  int rowsize = dst->GetRowSize(PLANAR_Y);
  int pitch = dst->GetPitch(PLANAR_Y);

  if (vi.ComponentSize() == 1) // 8bit
    fill_plane<BYTE>(dstp, vi.height, rowsize, pitch, 0x7e, env, cpu_mask_);
  else if (vi.ComponentSize() == 2) { // 16bit
    uint16_t luma_val = 0x7e << (vi.BitsPerComponent() - 8);
    fill_plane<uint16_t>(dstp, vi.height, rowsize, pitch, luma_val, env, cpu_mask_);
  } else { // 32bit(float)
    fill_plane<float>(dstp, vi.height, rowsize, pitch, 126.0f / 256, env, cpu_mask_);
  }

  return dst;
}

// AVS+: Combine planes free-style for all planar formats

} // namespace aif::filters::planes
