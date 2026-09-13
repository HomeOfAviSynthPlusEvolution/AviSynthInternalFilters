// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from AviSynthPlus avs_core/filters/planeswap.cpp.
#include "swap_uv.h"
#include "kernel_adapter.h"
namespace aif::filters::planes {
AVSValue __cdecl SwapUV::CreateSwapUV(AVSValue args, void*, IScriptEnvironment* env) {
  PClip p = args[0].AsClip();
  if (p->GetVideoInfo().NumComponents() == 1)
    return p;
  return new SwapUV(p, env);
}

SwapUV::SwapUV(PClip _child, IScriptEnvironment* env) : GenericVideoFilter(_child) {
  if (!vi.IsYUV() && !vi.IsYUVA())
    env->ThrowError("SwapUV: YUV or YUVA data only!");
}

PVideoFrame __stdcall SwapUV::GetFrame(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);

  if (vi.IsPlanar()) {
    // Abuse subframe to flip the UV plane pointers -- extremely fast but a bit naughty!
    // !! if offsets would be size_t, be cautious when you subtract two unsigned size_t variables
    const int uvoffset = src->GetOffset(PLANAR_V) - src->GetOffset(PLANAR_U); // very naughty - don't do this at home!!
    if (vi.NumComponents() == 4) {
      return env->SubframePlanarA(src, 0, src->GetPitch(PLANAR_Y), src->GetRowSize(PLANAR_Y), src->GetHeight(PLANAR_Y),
                                  uvoffset, -uvoffset, src->GetPitch(PLANAR_V), 0);
    } else {
      return env->SubframePlanar(src, 0, src->GetPitch(PLANAR_Y), src->GetRowSize(PLANAR_Y), src->GetHeight(PLANAR_Y),
                                 uvoffset, -uvoffset, src->GetPitch(PLANAR_V));
    }
  }

  // YUY2
  PVideoFrame dst = env->NewVideoFrameP(vi, &src);
  const BYTE* srcp = src->GetReadPtr();
  BYTE* dstp = dst->GetWritePtr();
  int src_pitch = src->GetPitch();
  int dst_pitch = dst->GetPitch();
  int rowsize = src->GetRowSize();
  {
    if (aif_planes_swap(srcp, src_pitch, dstp, dst_pitch, rowsize / 2, vi.height, cpu(env)))
      env->ThrowError("SwapUV: invalid YUY2 frame");
  }
  return dst;
}

} // namespace aif::filters::planes
