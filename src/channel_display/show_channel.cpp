// SPDX-License-Identifier: GPL-2.0-or-later
// Adapted from AviSynth layer.cpp.
#include "show_channel.h"
#include "kernel_adapter.h"
#include <string>
namespace aif::filters::channel_display {
ShowChannel::ShowChannel(PClip _child, const char* pixel_type, int _channel, IScriptEnvironment* env)
    : GenericVideoFilter(_child), cpu_mask_(cpu(env)), channel(_channel), input_type(_child->GetVideoInfo().pixel_type),
      pixelsize(_child->GetVideoInfo().ComponentSize()), bits_per_pixel(_child->GetVideoInfo().BitsPerComponent()) {
  static const char* const ShowText[7] = {"Blue", "Green", "Red", "Alpha", "Y", "U", "V"};

  input_type_is_packed_rgb = vi.IsRGB() && !vi.IsPlanar();
  input_type_is_planar_rgb = vi.IsPlanarRGB();
  input_type_is_planar_rgba = vi.IsPlanarRGBA();
  input_type_is_yuva = vi.IsYUVA();
  input_type_is_yuv = vi.IsYUV() && vi.IsPlanar();
  input_type_is_planar = vi.IsPlanar();

  int orig_channel = channel;

  // A channel
  if ((channel == 3) && !vi.IsRGB32() && !vi.IsRGB64() && !vi.IsPlanarRGBA() && !vi.IsYUVA())
    env->ThrowError("ShowAlpha: RGB32, RGB64, Planar RGBA or YUVA data only");

  // R, G, B channel
  if ((channel >= 0) && (channel <= 2) && !vi.IsRGB())
    env->ThrowError("Show%s: plane is valid only with RGB or planar RGB(A) source", ShowText[channel]);

  // Y, U, V channel (4,5,6)
  if ((channel >= 4) && (channel <= 6)) {
    if (!vi.IsYUV() && !vi.IsYUVA())
      env->ThrowError("Show%s: plane is valid only with YUV(A) source", ShowText[channel]);
    if (channel != 4 && vi.IsY())
      env->ThrowError("Show%s: invalid plane for greyscale source", ShowText[channel]);
    channel -= 4; // map to 0,1,2
  }

  int target_bits_per_pixel;

  const int orig_width = vi.width;
  const int orig_height = vi.height;

  if (input_type_is_yuv || input_type_is_yuva) {
    if (channel == 1 || channel == 2) // U or V: target can be smaller than Y
    {
      vi.width >>= vi.GetPlaneWidthSubsampling(PLANAR_U);
      vi.height >>= vi.GetPlaneHeightSubsampling(PLANAR_U);
    }
  }

  const bool empty_pixel_type = pixel_type == nullptr || *pixel_type == 0;

  if (!lstrcmpi(pixel_type, "rgb") || (vi.IsRGB() && empty_pixel_type)) {
    // target is RGB, rgb (packed) is adaptively 32 or 64 bits
    //                rgb (planar) is of any bit depths
    if (vi.IsPlanar()) {
      // YUV, planar RGB or Y
      // always alphaless planar RGB output
      switch (bits_per_pixel) {
        case 8:
          vi.pixel_type = VideoInfo::CS_RGBP8;
          break;
        case 10:
          vi.pixel_type = VideoInfo::CS_RGBP10;
          break;
        case 12:
          vi.pixel_type = VideoInfo::CS_RGBP12;
          break;
        case 14:
          vi.pixel_type = VideoInfo::CS_RGBP14;
          break;
        case 16:
          vi.pixel_type = VideoInfo::CS_RGBP16;
          break;
        case 32:
          vi.pixel_type = VideoInfo::CS_RGBPS;
          break;
      }
    } else if (vi.IsRGB()) {
      // packed RGB source
      switch (bits_per_pixel) {
        case 8:
          vi.pixel_type = VideoInfo::CS_BGR32;
          break; // bit-depth adaptive
        case 16:
          vi.pixel_type = VideoInfo::CS_BGR64;
          break;
        default:
          env->ThrowError("Show%s: source must be 8 or 16 bits", ShowText[orig_channel]);
      }
    } else {
      env->ThrowError("Show%s: unsupported source format", ShowText[orig_channel]);
    }
    target_bits_per_pixel = bits_per_pixel;
  } else if (!lstrcmpi(pixel_type, "yuv") || ((vi.IsYUV() || vi.IsYUVA()) && empty_pixel_type)) {
    // target is YUV, rgb (packed) is adaptively 32 or 64 bits
    //                rgb (planar) is of any bit depths
    //                YUV,Y: 420
    // RGB source, when only 'yuv' is given, convert to 444
    switch (bits_per_pixel) {
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
    target_bits_per_pixel = bits_per_pixel;
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
        env->ThrowError("Show%s: invalid pixel_type!", ShowText[orig_channel]);
    }
    // new output format
    vi.pixel_type = new_pixel_type;

    if (vi.IsYUY2()) {
      if (vi.width & 1) {
        env->ThrowError("Show%s: width must be mod 2 for yuy2", ShowText[orig_channel]);
      }
    }
    if (vi.Is420()) {
      if (vi.width & 1) {
        env->ThrowError("Show%s: width must be mod 2 for 4:2:0 target", ShowText[orig_channel]);
      }
      if (vi.height & 1) {
        env->ThrowError("Show%s: height must be mod 2 for 4:2:0 target", ShowText[orig_channel]);
      }
    }
    if (vi.Is422()) {
      if (vi.width & 1) {
        env->ThrowError("Show%s: width must be mod 2 for 4:2:2 target", ShowText[orig_channel]);
      }
    }
    if (vi.IsYV411()) {
      if (vi.width & 3) {
        env->ThrowError("Show%s: width must be mod 4 for 4:1:1 target", ShowText[orig_channel]);
      }
    }

    target_bits_per_pixel = vi.BitsPerComponent();
  }

  if (target_bits_per_pixel != bits_per_pixel)
    env->ThrowError("Show%s: source bit depth must be %d for %s", ShowText[orig_channel], target_bits_per_pixel,
                    pixel_type);

  target_hasalpha = vi.IsRGB32() || vi.IsRGB64() || vi.IsPlanarRGBA() || vi.IsYUVA();
  source_hasalpha = input_type == VideoInfo::CS_BGR32 || input_type == VideoInfo::CS_BGR64 ||
                    input_type_is_planar_rgba || input_type_is_yuva;

  if (target_hasalpha && source_hasalpha && (vi.width != orig_width || vi.height != orig_height)) {
    env->ThrowError("Show%s: subsampled source plane and alpha-aware source and destination format: alpha dimensions "
                    "must be the same",
                    ShowText[orig_channel]);
  }
}

PVideoFrame ShowChannel::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);

  // for planar these will be reread for proper plane
  const BYTE* srcp = src->GetReadPtr();
  const int height = src->GetHeight();
  const int pitch = src->GetPitch();
  const int rowsize = src->GetRowSize();

  const float chroma_center_f = 0.0f;

  if (input_type_is_packed_rgb) {
    PVideoFrame dst = env->NewVideoFrameP(vi, &src);

    if (!vi.IsRGB()) {
      // delete _Matrix when target is not an RGB
      auto props = env->getFramePropsRW(dst);
      env->propDeleteKey(props, "_Matrix");
    }

    const int source_rgb_step = source_hasalpha ? 4 : 3;
    const int w = rowsize / pixelsize / source_rgb_step;

    if (vi.IsRGB() && !vi.IsPlanar()) {
      // packed RGB to packed RGB
      BYTE* dstp = dst->GetWritePtr();
      const int dstpitch = dst->GetPitch();

      if (pixelsize == 1) {
        if (!source_hasalpha && !target_hasalpha)
          packed_to_packedrgb<uint8_t, false, false>(dstp, dstpitch, srcp, pitch, w, height, channel, env, cpu_mask_);
        else if (!source_hasalpha && target_hasalpha)
          packed_to_packedrgb<uint8_t, false, true>(dstp, dstpitch, srcp, pitch, w, height, channel, env, cpu_mask_);
        else if (source_hasalpha && !target_hasalpha)
          packed_to_packedrgb<uint8_t, true, false>(dstp, dstpitch, srcp, pitch, w, height, channel, env, cpu_mask_);
        else // if (source_hasalpha && target_hasalpha)
          packed_to_packedrgb<uint8_t, true, true>(dstp, dstpitch, srcp, pitch, w, height, channel, env, cpu_mask_);
      } else {
        if (!source_hasalpha && !target_hasalpha)
          packed_to_packedrgb<uint16_t, false, false>(dstp, dstpitch, srcp, pitch, w, height, channel, env, cpu_mask_);
        else if (!source_hasalpha && target_hasalpha)
          packed_to_packedrgb<uint16_t, false, true>(dstp, dstpitch, srcp, pitch, w, height, channel, env, cpu_mask_);
        else if (source_hasalpha && !target_hasalpha)
          packed_to_packedrgb<uint16_t, true, false>(dstp, dstpitch, srcp, pitch, w, height, channel, env, cpu_mask_);
        else // if (source_hasalpha && target_hasalpha)
          packed_to_packedrgb<uint16_t, true, true>(dstp, dstpitch, srcp, pitch, w, height, channel, env, cpu_mask_);
      }
    } else if (vi.pixel_type == VideoInfo::CS_YUY2) {
      // packed RGB to YUY2
      BYTE* dstp = dst->GetWritePtr();
      const int dstpitch = dst->GetPitch();

      render(srcp, pitch, nullptr, 0, {dstp, nullptr, nullptr, nullptr}, {dstpitch, 0, 0, 0}, w, height, 1,
             source_rgb_step, 2, channel, env, cpu_mask_);
    } else if (vi.IsYUV() || vi.IsYUVA() || vi.IsY()) {
      // packed RGB -> Y, YUV(A)
      BYTE* dstp = dst->GetWritePtr();
      int dstpitch = dst->GetPitch();

      BYTE* dstp_a = target_hasalpha ? dst->GetWritePtr(PLANAR_A) : nullptr;

      if (pixelsize == 1) {
        if (!source_hasalpha && !target_hasalpha)
          packed_to_luma_alpha<uint8_t, false, false>(dstp, dstp_a, dstpitch, srcp, pitch, w, height, channel, env,
                                                      cpu_mask_);
        else if (!source_hasalpha && target_hasalpha)
          packed_to_luma_alpha<uint8_t, false, true>(dstp, dstp_a, dstpitch, srcp, pitch, w, height, channel, env,
                                                     cpu_mask_);
        else if (source_hasalpha && !target_hasalpha)
          packed_to_luma_alpha<uint8_t, true, false>(dstp, dstp_a, dstpitch, srcp, pitch, w, height, channel, env,
                                                     cpu_mask_);
        else // if (source_hasalpha && target_hasalpha)
          packed_to_luma_alpha<uint8_t, true, true>(dstp, dstp_a, dstpitch, srcp, pitch, w, height, channel, env,
                                                    cpu_mask_);
      } else {
        // 16 bit
        if (!source_hasalpha && !target_hasalpha)
          packed_to_luma_alpha<uint16_t, false, false>(dstp, dstp_a, dstpitch, srcp, pitch, w, height, channel, env,
                                                       cpu_mask_);
        else if (!source_hasalpha && target_hasalpha)
          packed_to_luma_alpha<uint16_t, false, true>(dstp, dstp_a, dstpitch, srcp, pitch, w, height, channel, env,
                                                      cpu_mask_);
        else if (source_hasalpha && !target_hasalpha)
          packed_to_luma_alpha<uint16_t, true, false>(dstp, dstp_a, dstpitch, srcp, pitch, w, height, channel, env,
                                                      cpu_mask_);
        else // if (source_hasalpha && target_hasalpha)
          packed_to_luma_alpha<uint16_t, true, true>(dstp, dstp_a, dstpitch, srcp, pitch, w, height, channel, env,
                                                     cpu_mask_);
      }

      // fill chroma neutral
      if (!vi.IsY()) {
        int uvrowsize = dst->GetRowSize(PLANAR_U);
        int uvpitch = dst->GetPitch(PLANAR_U);
        int dstheight = dst->GetHeight(PLANAR_U);
        BYTE* dstp_u = dst->GetWritePtr(PLANAR_U);
        BYTE* dstp_v = dst->GetWritePtr(PLANAR_V);
        switch (pixelsize) {
          case 1:
            fill_chroma<BYTE>(dstp_u, dstp_v, dstheight, uvrowsize, uvpitch, (BYTE)0x80, env, cpu_mask_);
            break;
          case 2:
            fill_chroma<uint16_t>(dstp_u, dstp_v, dstheight, uvrowsize, uvpitch, 1 << (vi.BitsPerComponent() - 1), env,
                                  cpu_mask_);
            break;
          case 4:
            fill_chroma<float>(dstp_u, dstp_v, dstheight, uvrowsize, uvpitch, chroma_center_f, env, cpu_mask_);
            break;
        }
      }
    } else if (vi.IsPlanarRGB() || vi.IsPlanarRGBA()) { // packed RGB -> Planar RGB 8/16 bit
      BYTE* dstp_g = dst->GetWritePtr(PLANAR_G);
      BYTE* dstp_b = dst->GetWritePtr(PLANAR_B);
      BYTE* dstp_r = dst->GetWritePtr(PLANAR_R);
      int dstpitch = dst->GetPitch();

      BYTE* dstp_a = target_hasalpha ? dst->GetWritePtr(PLANAR_A) : nullptr;

      if (pixelsize == 1) {
        if (!source_hasalpha && !target_hasalpha)
          packed_to_planarrgb<uint8_t, false, false>(dstp_r, dstp_g, dstp_b, dstp_a, dstpitch, srcp, pitch, w, height,
                                                     channel, env, cpu_mask_);
        else if (!source_hasalpha && target_hasalpha)
          packed_to_planarrgb<uint8_t, false, true>(dstp_r, dstp_g, dstp_b, dstp_a, dstpitch, srcp, pitch, w, height,
                                                    channel, env, cpu_mask_);
        else if (source_hasalpha && !target_hasalpha)
          packed_to_planarrgb<uint8_t, true, false>(dstp_r, dstp_g, dstp_b, dstp_a, dstpitch, srcp, pitch, w, height,
                                                    channel, env, cpu_mask_);
        else // if (source_hasalpha && target_hasalpha)
          packed_to_planarrgb<uint8_t, true, true>(dstp_r, dstp_g, dstp_b, dstp_a, dstpitch, srcp, pitch, w, height,
                                                   channel, env, cpu_mask_);
      } else {
        // 16 bit
        if (!source_hasalpha && !target_hasalpha)
          packed_to_planarrgb<uint16_t, false, false>(dstp_r, dstp_g, dstp_b, dstp_a, dstpitch, srcp, pitch, w, height,
                                                      channel, env, cpu_mask_);
        else if (!source_hasalpha && target_hasalpha)
          packed_to_planarrgb<uint16_t, false, true>(dstp_r, dstp_g, dstp_b, dstp_a, dstpitch, srcp, pitch, w, height,
                                                     channel, env, cpu_mask_);
        else if (source_hasalpha && !target_hasalpha)
          packed_to_planarrgb<uint16_t, true, false>(dstp_r, dstp_g, dstp_b, dstp_a, dstpitch, srcp, pitch, w, height,
                                                     channel, env, cpu_mask_);
        else // if (source_hasalpha && target_hasalpha)
          packed_to_planarrgb<uint16_t, true, true>(dstp_r, dstp_g, dstp_b, dstp_a, dstpitch, srcp, pitch, w, height,
                                                    channel, env, cpu_mask_);
      }
    }
    return dst;
  } // end of packed rgb source

  if (input_type_is_planar_rgb || input_type_is_planar_rgba || input_type_is_yuv || input_type_is_yuva) {
    // planar source
    const int planesYUV[4] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
    const int planesRGB[4] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
    const int* planes = (input_type_is_planar_rgb || input_type_is_planar_rgba) ? planesRGB : planesYUV;
    int final_channel = channel;
    // RGB channels: B=0 G=1 R=2 (like packed)
    // Planar order: G=0 B=1 R=2
    if (input_type_is_planar_rgb || input_type_is_planar_rgba) {
      // exchange B and G
      if (channel == 0)
        final_channel = 1;
      else if (channel == 1)
        final_channel = 0;
    }
    const int plane = planes[final_channel];

    const BYTE* srcp = src->GetReadPtr(plane); // source plane
    const BYTE* srcp_a = source_hasalpha ? src->GetReadPtr(PLANAR_A) : nullptr;

    const int width = src->GetRowSize(plane) / pixelsize;
    const int height = src->GetHeight(plane);
    const int pitch = src->GetPitch(plane);

    if (vi.IsRGB() && !vi.IsPlanar()) {
      // planar RGBP/YUVA -> packed RGB
      PVideoFrame dst = env->NewVideoFrameP(vi, &src);
      BYTE* dstp = dst->GetWritePtr();
      const int dstpitch = dst->GetPitch();

      if (!input_type_is_planar_rgb && !input_type_is_planar_rgba) {
        auto props = env->getFramePropsRW(dst);
        // delete _Matrix and ChromaLocation when source is not RGB
        env->propDeleteKey(props, "_Matrix");
        env->propDeleteKey(props, "_ChromaLocation");
      }

      if (bits_per_pixel == 8) {
        if (target_hasalpha) {
          if (source_hasalpha)
            planar_to_packedrgb<uint8_t, true, true>(dstp, dstpitch, srcp, srcp_a, pitch, width, height, env,
                                                     cpu_mask_);
          else
            planar_to_packedrgb<uint8_t, false, true>(dstp, dstpitch, srcp, srcp_a, pitch, width, height, env,
                                                      cpu_mask_);
        } else {
          planar_to_packedrgb<uint8_t, false, false>(dstp, dstpitch, srcp, srcp_a, pitch, width, height, env,
                                                     cpu_mask_);
        }
      } else {
        // 16 bits
        if (target_hasalpha) {
          if (source_hasalpha)
            planar_to_packedrgb<uint16_t, true, true>(dstp, dstpitch, srcp, srcp_a, pitch, width, height, env,
                                                      cpu_mask_);
          else
            planar_to_packedrgb<uint16_t, false, true>(dstp, dstpitch, srcp, srcp_a, pitch, width, height, env,
                                                       cpu_mask_);
        } else {
          planar_to_packedrgb<uint16_t, false, false>(dstp, dstpitch, srcp, srcp_a, pitch, width, height, env,
                                                      cpu_mask_);
        }
      }

      return dst;
    } else if (vi.pixel_type == VideoInfo::CS_YUY2) // RGB(A)P/YUVA->YUY2
    {
      PVideoFrame dst = env->NewVideoFrameP(vi, &src);
      BYTE* dstp = dst->GetWritePtr();
      const int dstpitch = dst->GetPitch();

      auto props = env->getFramePropsRW(dst);
      env->propDeleteKey(props, "_Matrix");
      env->propDeleteKey(props, "_ChromaLocation");

      render(srcp, pitch, nullptr, 0, {dstp, nullptr, nullptr, nullptr}, {dstpitch, 0, 0, 0}, width, height, 1, 1, 2, 0,
             env, cpu_mask_);
      return dst;
    } else { // planar to planar
      // RGB(A)P/YUVA -> YV12/16/24/Y8 + 16bit
      PVideoFrame dst = env->NewVideoFrameP(vi, &src);

      // remove frame props if either src or target is not RGB
      if (!(input_type_is_planar_rgb || input_type_is_planar_rgba || vi.IsRGB())) {
        // RGB origin to YUV
        auto props = env->getFramePropsRW(dst);
        env->propDeleteKey(props, "_Matrix");
        if (input_type_is_yuv || input_type_is_yuva)
          env->propDeleteKey(props, "_ChromaLocation");
      }

      if (vi.IsYUV() || vi.IsYUVA() || vi.IsY()) // Y8, YV12, Y16, YUV420P16, etc.
      {
        BYTE* dstp = dst->GetWritePtr();
        int dstpitch = dst->GetPitch();

        // copy source plane to luma
        env->BitBlt(dstp, dstpitch, srcp, pitch, width * pixelsize, height);
        // fill UV with neutral
        if (!vi.IsY()) {
          int uvrowsize = dst->GetRowSize(PLANAR_U);
          int uvpitch = dst->GetPitch(PLANAR_U);
          int dstheight = dst->GetHeight(PLANAR_U);
          BYTE* dstp_u = dst->GetWritePtr(PLANAR_U);
          BYTE* dstp_v = dst->GetWritePtr(PLANAR_V);
          switch (pixelsize) {
            case 1:
              fill_chroma<uint8_t>(dstp_u, dstp_v, dstheight, uvrowsize, uvpitch, (uint8_t)0x80, env, cpu_mask_);
              break;
            case 2:
              fill_chroma<uint16_t>(dstp_u, dstp_v, dstheight, uvrowsize, uvpitch, 1 << (vi.BitsPerComponent() - 1),
                                    env, cpu_mask_);
              break;
            case 4:
              fill_chroma<float>(dstp_u, dstp_v, dstheight, uvrowsize, uvpitch, chroma_center_f, env, cpu_mask_);
              break;
          }
        }
      } else if (vi.IsPlanarRGB() || vi.IsPlanarRGBA()) { // RGBP(A)/YUVA -> Planar RGB
        BYTE* dstp_g = dst->GetWritePtr(PLANAR_G);
        BYTE* dstp_b = dst->GetWritePtr(PLANAR_B);
        BYTE* dstp_r = dst->GetWritePtr(PLANAR_R);

        int dstpitch = dst->GetPitch();
        int dstwidth = dst->GetRowSize() / pixelsize;

        // copy to all channels
        if (pixelsize == 1) {
          for (int i = 0; i < height; ++i) {
            for (int j = 0; j < dstwidth; ++j) {
              dstp_g[j] = dstp_b[j] = dstp_r[j] = srcp[j];
            }
            srcp += pitch;
            dstp_g += dstpitch;
            dstp_b += dstpitch;
            dstp_r += dstpitch;
          }
        } else if (pixelsize == 2) {
          for (int i = 0; i < height; ++i) {
            for (int j = 0; j < dstwidth; ++j) {
              reinterpret_cast<uint16_t*>(dstp_g)[j] = reinterpret_cast<uint16_t*>(dstp_b)[j] =
                  reinterpret_cast<uint16_t*>(dstp_r)[j] = reinterpret_cast<const uint16_t*>(srcp)[j];
            }
            srcp += pitch;
            dstp_g += dstpitch;
            dstp_b += dstpitch;
            dstp_r += dstpitch;
          }
        } else { // pixelsize==4
          for (int i = 0; i < height; ++i) {
            for (int j = 0; j < dstwidth; ++j) {
              reinterpret_cast<float*>(dstp_g)[j] = reinterpret_cast<float*>(dstp_b)[j] =
                  reinterpret_cast<float*>(dstp_r)[j] = reinterpret_cast<const float*>(srcp)[j];
            }
            srcp += pitch;
            dstp_g += dstpitch;
            dstp_b += dstpitch;
            dstp_r += dstpitch;
          }
        }
      }
      if (target_hasalpha) {
        // fill alpha with transparent
        const int dst_rowsizeA = dst->GetRowSize(PLANAR_A);
        const int dst_pitchA = dst->GetPitch(PLANAR_A);
        BYTE* dstp_a = dst->GetWritePtr(PLANAR_A);
        const int heightA = dst->GetHeight(PLANAR_A);

        if (source_hasalpha) {
          // copy source alpha plane to target alpha plane
          env->BitBlt(dstp_a, dst_pitchA, srcp_a, pitch, width * pixelsize, height);
        } else {
          switch (vi.ComponentSize()) {
            case 1:
              fill_plane<uint8_t>(dstp_a, heightA, dst_rowsizeA, dst_pitchA, 0xFF, env, cpu_mask_);
              break;
            case 2:
              fill_plane<uint16_t>(dstp_a, heightA, dst_rowsizeA, dst_pitchA, (1 << vi.BitsPerComponent()) - 1, env,
                                   cpu_mask_);
              break;
            case 4:
              fill_plane<float>(dstp_a, heightA, dst_rowsizeA, dst_pitchA, 1.0f, env, cpu_mask_);
              break;
          }
        }
      }
      return dst;
    }
  } // planar RGB(A) or YUVA source

  env->ThrowError("ShowChannel: unexpected end of function");
  return src;
}

AVSValue ShowChannel::Create(AVSValue args, void* channel, IScriptEnvironment* env) {
  // yuy2 is autoconverted to YV16
  PClip clip = args[0].AsClip();
  const VideoInfo& vi = clip->GetVideoInfo();

  if (vi.IsYUY2()) {
    AVSValue new_args[1] = {clip};
    clip = env->Invoke("ConvertToYV16", AVSValue(new_args, 1)).AsClip();
  }
  return new ShowChannel(clip, args[1].AsString(""), (int)(size_t)channel, env);
}

} // namespace aif::filters::channel_display
