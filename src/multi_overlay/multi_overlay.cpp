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

#include "multi_overlay.h"
namespace aif::filters::multi_overlay {
MultiOverlay::MultiOverlay(const std::vector<PClip>& child_array, const std::vector<int>& position_array,
                           IScriptEnvironment* env)
    : children(child_array), positions(position_array) {
  if (children.size() < 2)
    env->ThrowError("MultiOverlay: at least one overlay clip is required.");
  vi = children[0]->GetVideoInfo();

  // 0th is the base clip
  // 1.. the clips to overlay
  for (size_t i = 1; i < children.size(); ++i) {
    const VideoInfo& vin = children[i]->GetVideoInfo();
    if (!vi.IsSameColorspace(vin))
      env->ThrowError("MultiOverlay: image format must match with the base");
  }

  const size_t copy_paste_count = children.size() - 1;
  const size_t param_count = position_array.size();

  // either 2 values per clip
  //   target_x, target_y
  // or 6 values per clip
  //   target_x, target_y, src_x, src_y, src_width, src_height
  if (copy_paste_count * 2 != param_count && copy_paste_count * 6 != param_count)
    env->ThrowError("MultiOverlay: expected 2 or 6 values per source clip.");

  const bool grey = vi.IsY();
  const bool isRGBPfamily = vi.IsPlanarRGB() || vi.IsPlanarRGBA();

  const int shift_w = (vi.IsPlanar() && !grey && !isRGBPfamily) ? vi.GetPlaneWidthSubsampling(PLANAR_U) : 0;
  const int shift_h = (vi.IsPlanar() && !grey && !isRGBPfamily) ? vi.GetPlaneHeightSubsampling(PLANAR_U) : 0;
  const int mask_w = (1 << shift_w) - 1;
  const int mask_h = (1 << shift_h) - 1;

  auto params_per_clip = param_count / copy_paste_count;
  for (size_t i = 0; i < copy_paste_count; i++) {
    const VideoInfo& vin = children[i + 1]->GetVideoInfo();

    const int target_x = positions[i * params_per_clip + 0];
    const int target_y = positions[i * params_per_clip + 1];
    if (target_x & mask_w)
      env->ThrowError("MultiOverlay: target x must be mod %d for this video format", (1 << shift_w));
    if (target_y & mask_h)
      env->ThrowError("MultiOverlay: target y must be mod %d for this video format", (1 << shift_h));

    if (params_per_clip == 6) {
      // additional source position and dimensions
      const int src_x = positions[i * params_per_clip + 2];
      const int src_y = positions[i * params_per_clip + 3];
      if (src_x < 0 || src_y < 0)
        env->ThrowError("MultiOverlay: source coordinate cannot be negative.");

      const int src_w = positions[i * params_per_clip + 4];
      const int src_h = positions[i * params_per_clip + 5];
      if (src_w <= 0 || src_h <= 0)
        env->ThrowError("MultiOverlay: source width and height must be positive.");

      if (src_x & mask_w)
        env->ThrowError("MultiOverlay: source x must be mod %d for this video format", (1 << shift_w));
      if (src_y & mask_h)
        env->ThrowError("MultiOverlay: source y must be mod %d for this video format", (1 << shift_h));
      if (src_w & mask_w)
        env->ThrowError("MultiOverlay: source width must be mod %d for this video format", (1 << shift_w));
      if (src_h & mask_h)
        env->ThrowError("MultiOverlay: source height must be mod %d for this video format", (1 << shift_h));

      if (int64_t(src_x) + src_w > vin.width)
        env->ThrowError("MultiOverlay: copy exceeds clip width. x=%d, size=%d, clip width=%d", src_x, src_w, vin.width);
      if (int64_t(src_y) + src_h > vin.height)
        env->ThrowError("MultiOverlay: copy exceeds clip height. y=%d, size=%d, clip height=%d", src_y, src_h,
                        vin.height);
    }
  }
}

PVideoFrame __stdcall MultiOverlay::GetFrame(int n, IScriptEnvironment* env) {
  std::vector<PVideoFrame> frames;
  frames.reserve(children.size());

  for (const auto& child : children)
    frames.emplace_back(child->GetFrame(n, env));

  PVideoFrame dst = frames[0]; // multioverlay target clip
  env->MakeWritable(&dst);

  const size_t copy_paste_count = children.size() - 1;
  const size_t param_count = positions.size();
  const size_t params_per_clip = param_count / copy_paste_count; // 2 or 6
  const bool source_extra = params_per_clip == 6;

  // packed RGB is upside down
  const bool is_packed_rgb = vi.IsRGB24() || vi.IsRGB32() || vi.IsRGB48() || vi.IsRGB64();
  const int bfp = is_packed_rgb || vi.IsYUY2() ? vi.BytesFromPixels(1) : vi.ComponentSize();
  // also for packed RGBs, pixelsize is not enough

  const int planesPacked[] = {DEFAULT_PLANE, DEFAULT_PLANE, DEFAULT_PLANE, DEFAULT_PLANE};
  const int planesYUV[4] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
  const int planesRGB[4] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
  const int* planes = is_packed_rgb ? planesPacked : vi.IsY() || vi.IsYUV() || vi.IsYUVA() ? planesYUV : planesRGB;

  const int planecount = is_packed_rgb || vi.IsYUY2() ? 1 : vi.NumComponents();

  for (int p = 0; p < planecount; p++) {
    const int plane = planes[p];
    BYTE* dstp = dst->GetWritePtr(plane);
    const int dst_pitch = dst->GetPitch(plane);
    const int dst_height = dst->GetHeight(plane);

    // 0th was the target
    for (size_t i = 1; i < frames.size(); i++) {
      const auto& src = frames[i];
      const int src_rowsize = src->GetRowSize(plane);

      // these may be safely overridden
      int src_width = src_rowsize / bfp;
      int src_height = src->GetHeight(plane);

      const BYTE* srcp = src->GetReadPtr(plane);
      const int src_pitch = src->GetPitch(plane);

      // packed RGB a Y and all 0th plane is not subsampled.
      const int shift_w = p == 0 ? 0 : vi.GetPlaneWidthSubsampling(plane);
      const int shift_h = p == 0 ? 0 : vi.GetPlaneHeightSubsampling(plane);

      int64_t target_x = positions[(i - 1) * params_per_clip + 0] >> shift_w;
      int64_t target_y = positions[(i - 1) * params_per_clip + 1] >> shift_h;

      int dst_w = dst->GetRowSize(plane) / bfp;

      int64_t src_x = 0;
      int64_t src_y = 0;
      int64_t src_w = src_width;
      int64_t src_h = src_height;

      if (source_extra) {
        src_x = positions[(i - 1) * params_per_clip + 2] >> shift_w;
        src_y = positions[(i - 1) * params_per_clip + 3] >> shift_h;
        src_w = positions[(i - 1) * params_per_clip + 4] >> shift_w;
        src_h = positions[(i - 1) * params_per_clip + 5] >> shift_h;
      }
      // bring negative target coordinates to 0,
      // adjust source position and dimension accordingly
      if (target_x < 0) {
        src_x += -target_x;
        src_w -= -target_x;
        target_x = 0;
      }
      if (target_y < 0) {
        src_y += -target_y;
        src_h -= -target_y;
        target_y = 0;
      }

      // check and limit dimension parameters

      // Already checked at filter creation:
      // o coordinates and dimensions are subsampling friendly
      // o target x and y >= 0
      // o source width and height > 0
      // o src_x + src_width and src_y + src_height fit in source clip dimensions

      if (src_h > src_height)
        src_h = src_height;
      if (target_y + src_h > dst_height)
        src_h -= (target_y + src_h - dst_height);

      if (src_w > src_width)
        src_w = src_width;
      if (target_x + src_w > dst_w)
        src_w -= (target_x + src_w - dst_w);

      if (src_w > 0 && src_h > 0) {
        src_height = int(src_h);
        src_width = int(src_w * bfp);

        if (is_packed_rgb) {
          // start from bottom: packed RGB is upside down
          src_y = (src->GetHeight(plane) - src_y - 1) - (src_height - 1);
          target_y = (dst_height - target_y - 1) - (src_height - 1);
        }

        env->BitBlt(dstp + target_x * bfp + target_y * dst_pitch, dst_pitch, srcp + src_x * bfp + src_y * src_pitch,
                    src_pitch, src_width, src_height);
      }
    }
  }

  return dst;
}

AVSValue __cdecl MultiOverlay::Create(AVSValue args, void*, IScriptEnvironment* env) {
  std::vector<PClip> children;
  if (args[1].IsArray()) {
    children.resize(1 + args[1].ArraySize());

    children[0] = args[0].AsClip();
    for (int i = 1; i < (int)children.size(); ++i) // Copy clips
      children[i] = args[1][i - 1].AsClip();
  } else if (args[1].IsClip()) { // Make easy to call with trivial 2 clips
    children.resize(2);

    children[0] = args[0].AsClip();
    children[1] = args[1].AsClip();

  } else {
    env->ThrowError("MultiOverlay: clip array not recognized!");
    return 0;
  }

  std::vector<int> positions;
  if (!args[2].IsArray()) {
    env->ThrowError("MultiOverlay: position array not recognized!");
    return 0;
  }
  const int pos_count = args[2].ArraySize();
  if (size_t(pos_count) != 2 * (children.size() - 1) && size_t(pos_count) != 6 * (children.size() - 1))
    env->ThrowError("MultiOverlay: position array must contain 2 or 6 entries for each clip to overlay!");
  positions.resize(pos_count); // x,y for each overlay clip

  for (int i = 0; i < pos_count; ++i) // Copy positions, x, y, or x, y, src_x, src_y, src_width, src_height
    positions[i] = args[2][i].AsInt();

  return new MultiOverlay(children, positions, env);
}

} // namespace aif::filters::multi_overlay
