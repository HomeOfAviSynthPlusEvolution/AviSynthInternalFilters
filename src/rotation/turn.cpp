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

/*
** Turn. version 0.1
** (c) 2003 - Ernst Peché
**
*/

#include "turn.h"
#include "kernel_adapter.h"
namespace aif::filters::rotation {
enum { DIRECTION_LEFT, DIRECTION_RIGHT, DIRECTION_180 };
Turn::Turn(PClip c, int direction, IScriptEnvironment* env)
    : GenericVideoFilter(c), u_or_b_source(nullptr), v_or_r_source(nullptr) {
  if (vi.pixel_type & VideoInfo::CS_INTERLEAVED) {
    num_planes = 1;
  } else if (vi.IsPlanarRGBA() || vi.IsYUVA()) {
    num_planes = 4;
  } else {
    num_planes = vi.NumComponents();
  }

  splanes[0] = vi.IsRGB() ? PLANAR_G : PLANAR_Y;
  splanes[1] = vi.IsRGB() ? PLANAR_B : PLANAR_U;
  splanes[2] = vi.IsRGB() ? PLANAR_R : PLANAR_V;
  splanes[3] = PLANAR_A;

  if (direction != DIRECTION_180) {
    if (vi.IsYUY2() && (vi.height & 1)) {
      env->ThrowError("Turn: YUY2 data must have mod2 height.");
    }
    if (num_planes > 1) {
      int mod_h = vi.IsRGB() ? 1 : (1 << vi.GetPlaneWidthSubsampling(PLANAR_U));
      int mod_v = vi.IsRGB() ? 1 : (1 << vi.GetPlaneHeightSubsampling(PLANAR_U));
      if (mod_h != mod_v) {
        if (vi.width % mod_h) {
          env->ThrowError("Turn: Planar data must have MOD %d height.", mod_h);
        }
        if (vi.height % mod_v) {
          env->ThrowError("Turn: Planar data must have MOD %d width.", mod_v);
        }
        SetUVSource(mod_h, mod_v, env);
      }
    }
    int t = vi.width;
    vi.width = vi.height;
    vi.height = t;
  }

  bytes = pixel_bytes(vi);
  operation = direction;
  if (vi.IsRGB() && !vi.IsPlanar() && direction < 2)
    operation = 1 - direction;
}

void Turn::SetUVSource(int mod_h, int mod_v, IScriptEnvironment* env) {
  u_or_b_source = env->Invoke("UToY8", child).AsClip();
  v_or_r_source = env->Invoke("VToY8", child).AsClip();
  const auto& uv = u_or_b_source->GetVideoInfo();
  const int width = uv.width * mod_h / mod_v;
  const int height = uv.height * mod_v / mod_h;
  AVSValue u[] = {u_or_b_source, width, height, 1.0 / 3, 1.0 / 3};
  AVSValue v[] = {v_or_r_source, width, height, 1.0 / 3, 1.0 / 3};
  u_or_b_source = env->Invoke("BicubicResize", AVSValue(u, 5)).AsClip();
  v_or_r_source = env->Invoke("BicubicResize", AVSValue(v, 5)).AsClip();
  splanes[1] = 0;
  splanes[2] = 0;
}

int __stdcall Turn::SetCacheHints(int cachehints, int frame_range) {
  (void)frame_range;
  return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
}

PVideoFrame __stdcall Turn::GetFrame(int n, IScriptEnvironment* env) {
  const int dplanes[] = {
      0,
      vi.IsRGB() ? PLANAR_B : PLANAR_U,
      vi.IsRGB() ? PLANAR_R : PLANAR_V,
      PLANAR_A,
  };

  auto src = child->GetFrame(n, env);
  auto dst = env->NewVideoFrameP(vi, &src);

  PVideoFrame srcs[4] = {
      src,
      u_or_b_source ? u_or_b_source->GetFrame(n, env) : src,
      v_or_r_source ? v_or_r_source->GetFrame(n, env) : src,
      src,
  };

  for (int p = 0; p < num_planes; ++p) {
    const int splane = splanes[p];
    const int dplane = dplanes[p];
    apply(srcs[p], dst, splane, dplane, bytes, operation, env);
  }

  return dst;
}

AVSValue __cdecl Turn::create_turnleft(AVSValue args, void*, IScriptEnvironment* env) {
  return new Turn(args[0].AsClip(), DIRECTION_LEFT, env);
}

AVSValue __cdecl Turn::create_turnright(AVSValue args, void*, IScriptEnvironment* env) {
  return new Turn(args[0].AsClip(), DIRECTION_RIGHT, env);
}

AVSValue __cdecl Turn::create_turn180(AVSValue args, void*, IScriptEnvironment* env) {
  return new Turn(args[0].AsClip(), DIRECTION_180, env);
}
} // namespace aif::filters::rotation
