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

#include "layer.h"
#include "kernel_adapter.h"
#include "cpu_policy.h"
#include <climits>
#include <algorithm>
#ifdef _WIN32
#include <avs/win.h>
#else
#include <avs/posix.h>
#endif
namespace aif::filters::layer {
using std::min;
enum { PLACEMENT_MPEG2, PLACEMENT_MPEG1 };
static int getPlacement(const AVSValue& _placement, IScriptEnvironment* env) {
  const char* placement = _placement.AsString(0);

  if (placement) {
    if (!lstrcmpi(placement, "mpeg2"))
      return PLACEMENT_MPEG2;

    if (!lstrcmpi(placement, "mpeg1"))
      return PLACEMENT_MPEG1;

    env->ThrowError("Layer: Unknown chroma placement");
  }
  return PLACEMENT_MPEG2;
}

Layer::Layer(PClip _child1, PClip _child2, const char _op[], int _lev, int _x, int _y, int _t, bool _chroma,
             float _opacity, int _placement, IScriptEnvironment* env)
    : child1(_child1), child2(_child2), Op(_op), levelB(_lev), ofsX(_x), ofsY(_y), chroma(_chroma), opacity(_opacity),
      placement(_placement) {
  kernels_ = select_kernels(env);
  if (!kernels_)
    env->ThrowError("Layer: unavailable CPU target");
  const VideoInfo& vi1 = child1->GetVideoInfo();
  const VideoInfo& vi2 = child2->GetVideoInfo();

  if (vi1.pixel_type != vi2.pixel_type && !vi1.IsSameColorspace(vi2)) // i420 and YV12 are matched OK
    env->ThrowError("Layer: image formats don't match");

  vi = vi1;

  hasAlpha = vi.IsRGB32() || vi.IsRGB64() || vi.IsYUVA() || vi.IsPlanarRGBA();
  bits_per_pixel = vi.BitsPerComponent();

  if (_t < 0 || _t > 255)
    env->ThrowError("Layer: threshold must be between 0 and 255");

  const bool levelSpecified = levelB >= 0;
  const bool strengthSpecified = opacity >= 0.0f;

  if (levelSpecified && strengthSpecified)
    env->ThrowError("Layer: cannot specify both level and opacity");
  if (levelSpecified && bits_per_pixel == 32)
    env->ThrowError("Layer: cannot specify level for 32 bit float format");

  if (levelSpecified) {
    if (hasAlpha)
      opacity = (float)levelB / ((1 << bits_per_pixel) + 1); // gives 1.0f for 257 (@8bit) and 65537 (@16 bits)
    // originally levelB was used in formula: (alpha*level + 1) / range_size,
    // now level is calculated from opacity as: level = opacity * ((1 << bits_per_pixel) + 1)
    else
      opacity = (float)levelB / ((1 << bits_per_pixel)); // YUY2 or other non-Alpha, gives 1.0f for 256 (@8bit)
    // we'll calculate back the level as: level = opacity * ((1 << bits_per_pixel))
  } else if (!strengthSpecified)
    opacity = 1.0f;

  if (vi.IsRGB32() || vi.IsRGB64() || vi.IsRGB24() || vi.IsRGB48())
    ofsY = static_cast<int>(
        std::clamp<int64_t>(int64_t(vi.height) - vi2.height - ofsY, INT_MIN, INT_MAX)); // packed RGB is upside down
  else if ((vi.IsYUV() || vi.IsYUVA()) && !vi.IsY()) {
    // make offsets subsampling friendly
    // e.g. for YUY2: ofsX = ofsX & 0xFFFFFFFE; // X offset for YUY2 must be aligned on even pixels
    ofsX = ofsX & ~((1 << vi.GetPlaneWidthSubsampling(PLANAR_U)) - 1);
    ofsY = ofsY & ~((1 << vi.GetPlaneHeightSubsampling(PLANAR_U)) - 1);
  }

  cp_overlap overlap{};
  Check(cp_intersect(vi.width, vi.height, vi2.width, vi2.height, ofsX, ofsY, &overlap), env);
  xdest = overlap.base_x;
  ydest = overlap.base_y;
  xsrc = overlap.source_x;
  ysrc = overlap.source_y;
  xcount = overlap.width;
  ycount = overlap.height;

  if (!(!lstrcmpi(Op, "Mul") || !lstrcmpi(Op, "Add") || !lstrcmpi(Op, "Fast") || !lstrcmpi(Op, "Subtract") ||
        !lstrcmpi(Op, "Lighten") || !lstrcmpi(Op, "Darken")))
    env->ThrowError("Layer supports the following ops: Fast, Lighten, Darken, Add, Subtract, Mul");

  if (!chroma) {
    if (!lstrcmpi(Op, "Darken"))
      env->ThrowError("Layer: monochrome darken illegal op");
    if (!lstrcmpi(Op, "Lighten"))
      env->ThrowError("Layer: monochrome lighten illegal op");
    if (!lstrcmpi(Op, "Fast"))
      env->ThrowError("Layer: this mode not allowed in FAST; use ADD instead");
  }

  // autoscale ThresholdParam from 8 bit base
  // todo check validity
  if (bits_per_pixel == 32)
    ThresholdParam = _t; // n/a
  else
    ThresholdParam = _t << (bits_per_pixel - 8);
  ThresholdParam_f = _t / 255.0f;

  overlay_frames = vi2.num_frames;
}

PVideoFrame __stdcall Layer::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame base = child1->GetFrame(n, env);
  if (xcount <= 0 || ycount <= 0)
    return base;
  PVideoFrame source = child2->GetFrame(min(n, overlay_frames - 1), env);
  env->MakeWritable(&base);
  const auto operation = !lstrcmpi(Op, "Mul")        ? LayerOperation::Multiply
                         : !lstrcmpi(Op, "Subtract") ? LayerOperation::Subtract
                         : !lstrcmpi(Op, "Fast")     ? LayerOperation::Fast
                         : !lstrcmpi(Op, "Lighten")  ? LayerOperation::Lighten
                         : !lstrcmpi(Op, "Darken")   ? LayerOperation::Darken
                                                     : LayerOperation::Add;
  LayerFrame(base, source, vi, child2->GetVideoInfo(), {xdest, ydest, xsrc, ysrc, xcount, ycount}, operation, chroma,
             hasAlpha, opacity, bits_per_pixel == 32 ? ThresholdParam_f : ThresholdParam,
             placement == PLACEMENT_MPEG1 ? CP_CENTER : CP_MPEG2, env, kernels_);
  return base;
}

AVSValue __cdecl Layer::Create(AVSValue args, void*, IScriptEnvironment* env) {
  const VideoInfo& vi1 = args[0].AsClip()->GetVideoInfo();
  const VideoInfo& vi2 = args[1].AsClip()->GetVideoInfo();

  // convert old RGB format to planar RGB
  PClip clip1;
  if (vi1.IsRGB24() || vi1.IsRGB48()) {
    AVSValue new_args[1] = {args[0].AsClip()};
    clip1 = env->Invoke("ConvertToPlanarRGB", AVSValue(new_args, 1)).AsClip();
  }
  /* formats handled by Layer core
  else if (vi1.IsRGB32() || vi1.IsRGB64()) {
    AVSValue new_args[1] = { args[0].AsClip() };
    clip1 = env->Invoke("ConvertToPlanarRGBA", AVSValue(new_args, 1)).AsClip();
  }
  else if (vi1.IsYUY2()) {
    AVSValue new_args[1] = { args[0].AsClip() };
    clip1 = env->Invoke("ConvertToYV16", AVSValue(new_args, 1)).AsClip();
  }
  */
  else {
    clip1 = args[0].AsClip();
  }

  PClip clip2;
  if (vi2.IsRGB24() || vi2.IsRGB48()) {
    AVSValue new_args[1] = {args[1].AsClip()};
    clip2 = env->Invoke("ConvertToPlanarRGB", AVSValue(new_args, 1)).AsClip();
  }
  /* formats handled by Layer core
  else if (vi2.IsRGB32() || vi2.IsRGB64()) {
    AVSValue new_args[1] = { args[1].AsClip() };
    clip2 = env->Invoke("ConvertToPlanarRGBA", AVSValue(new_args, 1)).AsClip();
  }
  else if (vi_orig.IsYUY2()) {
    AVSValue new_args[1] = { args[1].AsClip() };
    clip2 = env->Invoke("ConvertToYV16", AVSValue(new_args, 1)).AsClip();
  }
  */
  else {
    clip2 = args[1].AsClip();
  }

  Layer* Result = new Layer(clip1, clip2, args[2].AsString("Add"), args[3].AsInt(-1), args[4].AsInt(0),
                            args[5].AsInt(0), args[6].AsInt(0), args[7].AsBool(true),
                            args[8].AsFloatf(-1.0f),    // opacity
                            getPlacement(args[9], env), // chroma placement
                            env);

  if (vi1.IsRGB24()) {
    AVSValue new_args2[1] = {Result};
    return env->Invoke("ConvertToRGB24", AVSValue(new_args2, 1)).AsClip();
  } else if (vi1.IsRGB48()) {
    AVSValue new_args2[1] = {Result};
    return env->Invoke("ConvertToRGB48", AVSValue(new_args2, 1)).AsClip();
  }
  /* formats handled by Layer core
  else if (vi1.IsRGB32()) {
    AVSValue new_args2[1] = { Result };
    return env->Invoke("ConvertToRGB32", AVSValue(new_args2, 1)).AsClip();
  }
  else if (vi1.IsRGB64()) {
    AVSValue new_args2[1] = { Result };
    return env->Invoke("ConvertToRGB64", AVSValue(new_args2, 1)).AsClip();
  }
  else if (vi1.IsYUY2()) {
    AVSValue new_args2[1] = { Result };
    return env->Invoke("ConvertToYUY2", AVSValue(new_args2, 1)).AsClip();
  }
  */

  return Result;
}

/**********************************
 *******   Subtract Filter   ******
 *********************************/

} // namespace aif::filters::layer
