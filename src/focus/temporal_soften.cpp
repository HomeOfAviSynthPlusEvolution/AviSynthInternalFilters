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

#include "temporal_soften.h"
#include "kernel_adapter.h"
#include <algorithm>
#include <vector>

namespace aif::filters::focus {
using std::clamp;
using std::min;

TemporalSoften::TemporalSoften(PClip _child, unsigned radius, unsigned luma_thresh, unsigned chroma_thresh,
                               int _scenechange, IScriptEnvironment* env)
    : GenericVideoFilter(_child), scenechange(_scenechange), luma_threshold(min(luma_thresh, 255u)),
      chroma_threshold(min(chroma_thresh, 255u)), kernel(2 * min(radius, (unsigned int)MAX_RADIUS) + 1) {

  child->SetCacheHints(CACHE_WINDOW, kernel);

  if (vi.IsRGB24() || vi.IsRGB48()) {
    env->ThrowError("TemporalSoften: RGB24/48 Not supported, use ConvertToRGB32/48().");
  }

  if ((vi.IsRGB32() || vi.IsRGB64()) && (vi.width & 1)) {
    env->ThrowError("TemporalSoften: RGB32/64 source must be multiple of 2 in width.");
  }

  if ((vi.IsYUY2()) && (vi.width & 3)) {
    env->ThrowError("TemporalSoften: YUY2 source must be multiple of 4 in width.");
  }

  if (scenechange >= 255) {
    scenechange = 0;
  }

  if (scenechange > 0 && (vi.IsRGB32() || vi.IsRGB64())) {
    env->ThrowError("TemporalSoften: Scenechange not available on RGB32/64");
  }

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();

  // original scenechange parameter always 0-255
  int factor;
  if (vi.IsPlanar()) // Y/YUV, no Planar RGB here
    factor = 1;      // bitdepth independent. sad normalizes
  else
    factor = vi.BytesFromPixels(1) / pixelsize; // /pixelsize: correction for packed 16 bit rgb
  scenechange *= static_cast<int64_t>((vi.width / 32) * 32) * vi.height * factor;

  int c = 0;
  if (vi.IsPlanar() && (vi.IsYUV() || vi.IsYUVA())) {
    if (luma_thresh > 0) {
      planes[c].planeId = PLANAR_Y;
      planes[c++].threshold = luma_threshold;
    }
    if (chroma_thresh > 0 && vi.NumComponents() > 1) {
      planes[c].planeId = PLANAR_V;
      planes[c++].threshold = chroma_threshold;
      planes[c].planeId = PLANAR_U;
      planes[c++].threshold = chroma_threshold;
    }
  } else if (vi.IsYUY2()) {
    planes[c].planeId = 0;
    planes[c++].threshold = luma_thresh | (chroma_thresh << 8);
  } else if (vi.IsRGB()) { // For RGB We use Luma.
    if (vi.IsPlanar()) {
      planes[c].planeId = PLANAR_G;
      planes[c++].threshold = luma_threshold;
      planes[c].planeId = PLANAR_B;
      planes[c++].threshold = luma_threshold;
      planes[c].planeId = PLANAR_R;
      planes[c++].threshold = luma_threshold;
    } else { // packed RGB
      planes[c].planeId = 0;
      planes[c++].threshold = luma_threshold;
    }
  }
  plane_count = c;
  planes[c].planeId = 0;
}

PVideoFrame TemporalSoften::GetFrame(int n, IScriptEnvironment* env) {
  int radius = (kernel - 1) / 2;
  int c = 0;

  // Just skip if silly settings

  if ((!luma_threshold && !chroma_threshold) || !radius || !plane_count) {
    PVideoFrame ret = child->GetFrame(n, env); // P.F.
    return ret;
  }

  bool planeDisabled[16];

  for (int p = 0; p < 16; p++) {
    planeDisabled[p] = false;
  }

  std::vector<PVideoFrame> frames;
  frames.reserve(kernel);

  for (int p = n - radius; p <= n + radius; ++p) {
    frames.emplace_back(child->GetFrame(clamp(p, 0, vi.num_frames - 1), env));
  }

  // P.F. 16.04.06 leak fix r1841 after 8 days of bug chasing:
  // Reason #1 of the random QTGMC memory leaks (stuck frame refcounts)
  // MakeWritable alters the pointer if it is not yet writeable, thus the original PVideoFrame won't be freed
  // (refcount decremented) To fix this, we leave the frame[] array in its place and copy frame[radius] to
  // CenterFrame and make further write operations on this new frame. env->MakeWritable(&frames[radius]); //
  // old culprit line. if not yet writeable -> gives another pointer
  PVideoFrame CenterFrame = frames[radius];
  env->MakeWritable(&CenterFrame);

  do {
    const BYTE* planeP[16];
    const BYTE* planeP2[16];
    int planePitch[16];
    int planePitch2[16];

    int current_thresh = planes[c].threshold; // Threshold for current plane.
    int d = 0;
    for (int i = 0; i < radius; i++) { // Fetch all planes sequencially
      planePitch[d] = frames[i]->GetPitch(planes[c].planeId);
      planeP[d++] = frames[i]->GetReadPtr(planes[c].planeId);
    }

    //    BYTE* c_plane = frames[radius]->GetWritePtr(planes[c]);
    BYTE* c_plane = CenterFrame->GetWritePtr(planes[c].planeId); // P.F. using CenterFrame for write access

    for (int i = 1; i <= radius; i++) { // Fetch all planes sequencially
      planePitch[d] = frames[radius + i]->GetPitch(planes[c].planeId);
      planeP[d++] = frames[radius + i]->GetReadPtr(planes[c].planeId);
    }

    int rowsize = CenterFrame->GetRowSize(planes[c].planeId);
    int h = CenterFrame->GetHeight(planes[c].planeId);
    int pitch = CenterFrame->GetPitch(planes[c].planeId);

    if (scenechange > 0) {
      int d2 = 0;
      bool skiprest = false;
      for (int i = radius - 1; i >= 0; i--) { // Check frames backwards
        if ((!skiprest) && (!planeDisabled[i])) {
          int64_t sad = 0;
          checked(aif_focus_sad(c_plane, planeP[i], pitch, planePitch[i], rowsize, h, bits_per_pixel, allowed_cpu(env),
                                &sad),
                  env);
          if (sad < scenechange) {
            planePitch2[d2] = planePitch[i];
            planeP2[d2++] = planeP[i];
          } else {
            skiprest = true;
          }
          planeDisabled[i] = skiprest; // Disable this frame on next plane (so that Y can affect UV)
        } else {
          planeDisabled[i] = true;
        }
      }
      skiprest = false;
      for (int i = radius; i < 2 * radius; i++) { // Check forward frames
        if ((!skiprest) && (!planeDisabled[i])) { // Disable this frame on next plane (so that Y can affect UV)
          int64_t sad = 0;
          checked(aif_focus_sad(c_plane, planeP[i], pitch, planePitch[i], rowsize, h, bits_per_pixel, allowed_cpu(env),
                                &sad),
                  env);
          if (sad < scenechange) {
            planePitch2[d2] = planePitch[i];
            planeP2[d2++] = planeP[i];
          } else {
            skiprest = true;
          }
          planeDisabled[i] = skiprest;
        } else {
          planeDisabled[i] = true;
        }
      }

      // Copy back
      for (int i = 0; i < d2; i++) {
        planeP[i] = planeP2[i];
        planePitch[i] = planePitch2[i];
      }
      d = d2;
    }

    if (d < 1) {
      // Memory leak reason #2 r1841: this wasn't here before return
      for (int i = 0; i < kernel; ++i)
        frames[i] = nullptr;
      // return frames[radius];
      return CenterFrame; // return the modified frame
    }

    if (current_thresh) {
      // for threshold==255 -> simple average
      for (int y = 0; y < h; y++) { // One line at the time
        if (vi.IsYUY2()) {
          checked(aif_focus_temporal_line(c_plane, planeP, d, rowsize, bits_per_pixel, AIF_FOCUS_YUY2, luma_threshold,
                                          chroma_threshold, allowed_cpu(env)),
                  env);
        } else {
          checked(aif_focus_temporal_line(c_plane, planeP, d, rowsize, bits_per_pixel, AIF_FOCUS_PLANAR, current_thresh,
                                          current_thresh, allowed_cpu(env)),
                  env);
        }
        for (int p = 0; p < d; p++)
          planeP[p] += planePitch[p];
        c_plane += pitch;
      }
    } else { // Just maintain the plane
    }
    c++;
  } while (planes[c].planeId);

  //  PVideoFrame result = frames[radius]; // we are using CenterFrame instead
  //  return result;
  return CenterFrame;
}

AVSValue __cdecl TemporalSoften::Create(AVSValue args, void*, IScriptEnvironment* env) {
  return new TemporalSoften(args[0].AsClip(), args[1].AsInt(), args[2].AsInt(), args[3].AsInt(), args[4].AsInt(0),
                            /*args[5].AsInt(1),*/ env); // ignore mode parameter
}

} // namespace aif::filters::focus
