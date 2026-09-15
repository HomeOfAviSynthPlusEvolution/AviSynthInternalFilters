// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://avisynth.nl

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

// Avisynth filter: Layer
// by "poptones" (poptones@myrealbox.com)

#include "subtract.h"
#include <algorithm>
#include <type_traits>
namespace aif::filters::layer {
using std::min;
using std::max;
using std::clamp;
#include "kernel/subtract.h"
const std::array<BYTE, 513> Subtract::LUT_Diff8 = difference_table();
Subtract::Subtract(PClip _child1, PClip _child2, IScriptEnvironment* env) : child1(_child1), child2(_child2) {
  VideoInfo vi1 = child1->GetVideoInfo();
  VideoInfo vi2 = child2->GetVideoInfo();

  if (vi1.width != vi2.width || vi1.height != vi2.height)
    env->ThrowError("Subtract: image dimensions don't match");

  if (!(vi1.IsSameColorspace(vi2)))
    env->ThrowError("Subtract: image formats don't match");

  vi = vi1;
  vi.num_frames = max(vi1.num_frames, vi2.num_frames);
  vi.num_audio_samples = max(vi1.num_audio_samples, vi2.num_audio_samples);

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();
}

PVideoFrame __stdcall Subtract::GetFrame(int n, IScriptEnvironment* env) {
  int n1 = min(max(0, n), child1->GetVideoInfo().num_frames - 1);
  int n2 = min(max(0, n), child2->GetVideoInfo().num_frames - 1);
  PVideoFrame src1 = child1->GetFrame(n1, env);
  PVideoFrame src2 = child2->GetFrame(n2, env);

  env->MakeWritable(&src1);

  BYTE* src1p = src1->GetWritePtr();
  const BYTE* src2p = src2->GetReadPtr();
  int row_size = src1->GetRowSize();
  int src1_pitch = src1->GetPitch();
  int src2_pitch = src2->GetPitch();

  int width = row_size / pixelsize;
  int height = vi.height;

  if (vi.IsPlanar() && (vi.IsYUV() || vi.IsYUVA())) {
    // alpha
    if (pixelsize == 1) {
      // LUT is a bit faster than clamp version
      for (int y = 0; y < vi.height; y++) {
        for (int x = 0; x < row_size; x++) {
          src1p[x] = LUT_Diff8[src1p[x] - src2p[x] + 126 + 129];
        }
        src1p += src1->GetPitch();
        src2p += src2->GetPitch();
      }
    } else if (pixelsize == 2)
      subtract_plane<uint16_t, 126, false>(src1p, src2p, src1_pitch, src2_pitch, width, height, bits_per_pixel);
    else //if (pixelsize==4)
      subtract_plane<float, 126, false>(src1p, src2p, src1_pitch, src2_pitch, width, height, bits_per_pixel);

    // chroma
    row_size = src1->GetRowSize(PLANAR_U);
    if (row_size) {
      width = row_size / pixelsize;
      height = src1->GetHeight(PLANAR_U);
      src1_pitch = src1->GetPitch(PLANAR_U);
      src2_pitch = src2->GetPitch(PLANAR_U);
      // U_plane exists
      BYTE* src1p = src1->GetWritePtr(PLANAR_U);
      const BYTE* src2p = src2->GetReadPtr(PLANAR_U);
      BYTE* src1pV = src1->GetWritePtr(PLANAR_V);
      const BYTE* src2pV = src2->GetReadPtr(PLANAR_V);

      if (pixelsize == 1) {
        // LUT is a bit faster than clamp version
        for (int y = 0; y < height; y++) {
          for (int x = 0; x < width; x++) {
            src1p[x] = LUT_Diff8[src1p[x] - src2p[x] + 128 + 129];
            src1pV[x] = LUT_Diff8[src1pV[x] - src2pV[x] + 128 + 129];
          }
          src1p += src1_pitch;
          src2p += src2_pitch;
          src1pV += src1_pitch;
          src2pV += src2_pitch;
        }
      } else if (pixelsize == 2) {
        subtract_plane<uint16_t, 128, true>(src1p, src2p, src1_pitch, src2_pitch, width, height, bits_per_pixel);
        subtract_plane<uint16_t, 128, true>(src1pV, src2pV, src1_pitch, src2_pitch, width, height, bits_per_pixel);
      } else { //if (pixelsize==4)
        subtract_plane<float, 128, true>(src1p, src2p, src1_pitch, src2_pitch, width, height, bits_per_pixel);
        subtract_plane<float, 128, true>(src1pV, src2pV, src1_pitch, src2_pitch, width, height, bits_per_pixel);
      }
    }
    return src1;
  } // End planar YUV

  // For YUY2, 50% gray is about (126,128,128) instead of (128,128,128).  Grr...
  if (vi.IsYUY2()) {
    for (int y = 0; y < vi.height; ++y) {
      for (int x = 0; x < row_size; x += 2) {
        src1p[x] = LUT_Diff8[src1p[x] - src2p[x] + 126 + 129];
        src1p[x + 1] = LUT_Diff8[src1p[x + 1] - src2p[x + 1] + 128 + 129];
      }
      src1p += src1->GetPitch();
      src2p += src2->GetPitch();
    }
  } else { // RGB
    if (vi.IsPlanarRGB() || vi.IsPlanarRGBA()) {
      const int planesRGB[4] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};

      // do not diff Alpha
      for (int p = 0; p < 3; p++) {
        const int plane = planesRGB[p];
        src1p = src1->GetWritePtr(plane);
        src2p = src2->GetReadPtr(plane);
        src1_pitch = src1->GetPitch(plane);
        src2_pitch = src2->GetPitch(plane);
        if (pixelsize == 1)
          subtract_plane<uint8_t, 128, false>(src1p, src2p, src1_pitch, src2_pitch, width, height, bits_per_pixel);
        else if (pixelsize == 2)
          subtract_plane<uint16_t, 128, false>(src1p, src2p, src1_pitch, src2_pitch, width, height, bits_per_pixel);
        else
          subtract_plane<float, 128, false>(src1p, src2p, src1_pitch, src2_pitch, width, height, bits_per_pixel);
      }
    } else { // packed RGB
      if (pixelsize == 1) {
        for (int y = 0; y < vi.height; ++y) {
          for (int x = 0; x < row_size; ++x)
            src1p[x] = LUT_Diff8[src1p[x] - src2p[x] + 128 + 129];

          src1p += src1->GetPitch();
          src2p += src2->GetPitch();
        }
      } else { // pixelsize == 2: RGB48, RGB64
        // width is getrowsize based here: ok.
        subtract_plane<uint16_t, 128, false>(src1p, src2p, src1_pitch, src2_pitch, width, height, bits_per_pixel);
      }
    }
  }
  return src1;
}

AVSValue __cdecl Subtract::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new Subtract(args[0].AsClip(), args[1].AsClip(), env);
}

} // namespace aif::filters::layer
