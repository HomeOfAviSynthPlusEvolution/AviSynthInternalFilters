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

#include <avisynth.h>
#include <cmath>
#include <cstring>
#include <algorithm>
#ifdef AVS_WINDOWS
#include <avs/win.h>
#else
#include <avs/posix.h>
#endif
#include <avs/minmax.h>
#include "rational.h"
#include "convert_fps.h"
#include "cpu_policy.h"
#include "kernel/blend.h"
namespace aif::filters::frame_rate {
ConvertFPS::ConvertFPS(PClip _child, unsigned new_numerator, unsigned new_denominator, int _zone, int _vbi,
                       IScriptEnvironment* env)
    : GenericVideoFilter(_child), kernels_(select_kernels(env)), zone(_zone), vbi(_vbi), lps(0) {
  ValidateFPS("ConvertFPS: source", vi.fps_numerator, vi.fps_denominator, env);
  ValidateFPS("ConvertFPS: target", new_numerator, new_denominator, env);

  if (new_numerator == 0 || new_denominator == 0)
    env->ThrowError("ConvertFPS: Numerator and denominator must be positive.");
  if (vi.fps_numerator == 0 || vi.fps_denominator == 0)
    env->ThrowError("ConvertFPS: Source frame rate must be positive.");

  if (zone >= 0 && !vi.IsYUY2()) // Tritical Jan 2006
    env->ThrowError("ConvertFPS: zone >= 0 requires YUY2 input");

  FrameRatio("ConvertFPS", vi.fps_numerator, vi.fps_denominator, new_numerator, new_denominator, fa, fb, env);
  if (zone >= 0) {
    if (vbi < 0)
      vbi = 0;
    if (vbi > vi.height)
      vbi = vi.height;
    const uint64_t lines = (uint64_t(vi.height) + unsigned(vbi)) * uint64_t(fb) / uint64_t(fa);
    if (lines < 1 || lines > INT32_MAX)
      env->ThrowError("ConvertFPS: Scan-line advance must be between 1 and 2147483647; adjust FPS or vbi.");
    lps = int(lines);
    if (zone > lps)
      env->ThrowError("ConvertFPS: 'zone' too large. Maximum allowed %d", lps);
  } else if (3 * fb < (fa << 1)) {
    int dec = MulDiv(vi.fps_numerator, 20000, vi.fps_denominator);
    env->ThrowError("ConvertFPS: New frame rate too small. Must be greater than %d.%04d "
                    "Increase or use 'zone='",
                    dec / 30000, (dec / 3) % 10000);
  }
  vi.SetFPS(new_numerator, new_denominator);
  const int64_t num_frames = (vi.num_frames * fb + (fa >> 1)) / fa;
  if (num_frames > 0x7FFFFFFF) // MAXINT
    env->ThrowError("ConvertFPS: Maximum number of frames exceeded.");

  vi.num_frames = int(num_frames);
}

PVideoFrame __stdcall ConvertFPS::GetFrame(int n, IScriptEnvironment* env) {
  // Using int64 modulo instead of modf, for double holds only 53 bits
  // n*fa worst-like case: (60000/1001 <> 30000/1001)
  // n = 0x7FFFFFFF; // 31 bits
  // fa = 1001 * 60000ULL // 26 bits
  // summa 57 bits, too much, modf((double)n * fa / fb, &nsrc_f); is not enough
  int64_t modulo = (n * fa) % fb;
  double frac_f = (double)modulo / fb;
  int nsrc = int(n * fa / fb);

  if (zone < 0) {

    // Mode 1: Blend full frames

    constexpr double threshold_f = 1.0 / 16.0;
    // was: 1 << (resolution - 4); // 64/1024

    // Don't bother if the blend ratio is small
    if (frac_f < threshold_f)
      return child->GetFrame(nsrc, env);

    if (frac_f > 1.0 - threshold_f)
      return child->GetFrame(nsrc + 1, env);

    PVideoFrame a = child->GetFrame(nsrc, env);
    PVideoFrame b = child->GetFrame(nsrc + 1, env);

    env->MakeWritable(&a);

    const int planes_y[4] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
    const int planes_r[4] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
    const int* planes;

    int planeCount;
    planeCount = vi.IsPlanar() ? vi.NumComponents() : 1;
    planes = (!vi.IsPlanar() || vi.IsYUV() || vi.IsYUVA()) ? planes_y : planes_r;

    const int bits_per_pixel = vi.BitsPerComponent();
    for (int j = 0; j < planeCount; ++j) {
      const int plane = planes[j];
      const BYTE* b_data = b->GetReadPtr(plane);
      int b_pitch = b->GetPitch(plane);
      BYTE* a_data = a->GetWritePtr(plane);
      int a_pitch = a->GetPitch(plane);
      int row_size = a->GetRowSize(plane);
      int height = a->GetHeight(plane);

      float weight = (float)frac_f; // between 0 and 1.0

      MergePlane(kernels_, a_data, b_data, a_pitch, b_pitch, row_size, height, bits_per_pixel, weight, env);
    }

    return a;

  } else {
    // Mode 2: Switch to next frame at the scan line corresponding to the source frame's timing.
    // If zone > 0, perform a gradual transition, i.e. blend one frame into the next
    // over the given number of lines.

    PVideoFrame a = child->GetFrame(nsrc, env);
    PVideoFrame b = child->GetFrame(nsrc + 1, env);
    const BYTE* b_data = b->GetReadPtr();
    int b_pitch = b->GetPitch();
    const int row_size = a->GetRowSize();
    const int height = a->GetHeight();

    BYTE* pd;
    const BYTE *pa, *pb, *a_data = a->GetReadPtr();
    int a_pitch = a->GetPitch();

    int64_t switch_line = (int64_t)(lps * (1.0 - frac_f));
    int64_t top = switch_line - (zone >> 1);
    int64_t bottom = switch_line + (zone >> 1) - lps;
    if (bottom > 0 && nsrc > 0) {
      // Finish the transition from the previous frame
      switch_line -= lps;
      top -= lps;
      nsrc--;
      b = a;
      a = child->GetFrame(nsrc, env);
      b_pitch = a_pitch;
      b_data = a_data;
      a_data = a->GetReadPtr();
      a_pitch = a->GetPitch();
    } else if (top >= height)
      return a;

    // Result goes into a new buffer since it can be made up of a number of source frames
    PVideoFrame d = env->NewVideoFrameP(vi, &a);
    BYTE* data = d->GetWritePtr();
    const int pitch = d->GetPitch();
    if (top > 0)
      env->BitBlt(data, pitch, a_data, a_pitch, row_size, int(top));
  loop:
    bottom = std::min<int64_t>(switch_line + (zone >> 1), height);
    int safe_top = int(std::max<int64_t>(top, 0));
    pd = data + safe_top * pitch;
    pa = a_data + safe_top * a_pitch;
    pb = b_data + safe_top * b_pitch;
    for (int y = safe_top; y < bottom; y++) {
      int64_t scale = y - top;
      blend_scanline(pd, pa, pb, row_size, scale, zone);
      pd += pitch;
      pa += a_pitch;
      pb += b_pitch;
    }
    switch_line += lps;
    top = switch_line - (zone >> 1);
    int limit = int(std::min<int64_t>(height, top));
    if (bottom < limit) {
      pd = data + bottom * pitch;
      pb = b_data + bottom * b_pitch;
      env->BitBlt(pd, pitch, pb, b_pitch, row_size, int(limit - bottom));
    }
    if (top < height) {
      nsrc++;
      a = b;
      b = child->GetFrame(nsrc + 1, env);
      a_pitch = b_pitch;
      b_pitch = b->GetPitch();
      a_data = b_data;
      b_data = b->GetReadPtr();
      goto loop;
    }
    return d;
  }
}

bool __stdcall ConvertFPS::GetParity(int n) {
  if (vi.IsFieldBased())
    return child->GetParity(0) ^ (n & 1);
  else
    return child->GetParity(0);
}

AVSValue __cdecl ConvertFPS::Create(AVSValue args, void*, IScriptEnvironment* env) {
  if (args[1].AsInt() <= 0 || args[2].AsInt(1) <= 0)
    env->ThrowError("ConvertFPS: Numerator and denominator must be positive.");
  return new ConvertFPS(args[0].AsClip(), args[1].AsInt(), args[2].AsInt(1), args[3].AsInt(-1), args[4].AsInt(0), env);
}

AVSValue __cdecl ConvertFPS::CreateFloat(AVSValue args, void*, IScriptEnvironment* env) {
  uint32_t num, den;

  FloatToFPS("ConvertFPS", (float)args[1].AsFloat(), num, den, env);
  return new ConvertFPS(args[0].AsClip(), num, den, args[2].AsInt(-1), args[3].AsInt(0), env);
}

// Tritical Jan 2006
AVSValue __cdecl ConvertFPS::CreatePreset(AVSValue args, void*, IScriptEnvironment* env) {
  uint32_t num, den;

  PresetToFPS("ConvertFPS", args[1].AsString(), num, den, env);
  return new ConvertFPS(args[0].AsClip(), num, den, args[2].AsInt(-1), args[3].AsInt(0), env);
}

AVSValue __cdecl ConvertFPS::CreateFromClip(AVSValue args, void*, IScriptEnvironment* env) {
  const VideoInfo& vi = args[1].AsClip()->GetVideoInfo();

  if (!vi.HasVideo()) {
    env->ThrowError("ConvertFPS: The clip supplied to get the FPS from must contain video.");
  }

  return new ConvertFPS(args[0].AsClip(), vi.fps_numerator, vi.fps_denominator, args[2].AsInt(-1), args[3].AsInt(0),
                        env);
}
} // namespace aif::filters::frame_rate
