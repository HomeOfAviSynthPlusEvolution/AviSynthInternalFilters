// SPDX-License-Identifier: GPL-2.0-or-later
#include "letterbox.h"
#include "crop.h"
#include "add_borders.h"
#include <algorithm>
namespace aif::filters::crop {
AVSValue __cdecl create_letterbox(AVSValue args, void*, IScriptEnvironment* env) {
  PClip clip = args[0].AsClip();
  int top = args[1].AsInt();
  int bottom = args[2].AsInt();
  int left = args[3].AsInt(0);
  int right = args[4].AsInt(0);
  int color = args[5].AsInt(0);
  const VideoInfo& vi = clip->GetVideoInfo();

  // [0][1][2][3]   [4]   [5]        [6]          [7]       [8]       [9]      [10]  [11]
  //  c  i  i [x1]i [x2]i [color]i [color_yuv]i[resample]s[param1]f[param2]f[param3]f[r]i
  //   top, bottom, [left], [right] [,color] [,color_yuv]

  // similar to BlankClip/AddBorders
  bool color_as_yuv = false;
  if (args[6].Defined()) {
    if (color != 0) // Not quite 100% test
      env->ThrowError("LetterBox: color and color_yuv are mutually exclusive");

    if (!vi.IsYUV() && !vi.IsYUVA())
      env->ThrowError("LetterBox: color_yuv only valid for YUV color spaces");
    color = args[6].AsInt(); // override
    color_as_yuv = true;
  }

  if ((top < 0) || (bottom < 0) || (left < 0) || (right < 0))
    env->ThrowError("LetterBox: You cannot specify letterboxing less than 0.");
  if (top + bottom >= vi.height) // Must be >= otherwise it is interpreted wrong by crop()
    env->ThrowError("LetterBox: You cannot specify letterboxing that is bigger than the picture (height).");
  if (right + left >= vi.width) // Must be >= otherwise it is interpreted wrong by crop()
    env->ThrowError("LetterBox: You cannot specify letterboxing that is bigger than the picture (width).");

  if (vi.IsYUV() || vi.IsYUVA()) {
    int xsub = 0;
    int ysub = 0;

    if (vi.NumComponents() > 1) {
      xsub = vi.GetPlaneWidthSubsampling(PLANAR_U);
      ysub = vi.GetPlaneHeightSubsampling(PLANAR_U);
    }
    const int xmask = (1 << xsub) - 1;
    const int ymask = (1 << ysub) - 1;

    // YUY2, etc, ... can only operate to even pixel boundaries
    if (left & xmask)
      env->ThrowError("LetterBox: YUV images width must be divideable by %d (left side).", xmask + 1);
    if (right & xmask)
      env->ThrowError("LetterBox: YUV images width must be divideable by %d (right side).", xmask + 1);

    if (top & ymask)
      env->ThrowError("LetterBox: YUV images height must be divideable by %d (top).", ymask + 1);
    if (bottom & ymask)
      env->ThrowError("LetterBox: YUV images height must be divideable by %d (bottom).", ymask + 1);
  }

  left = std::max(0, left);
  top = std::max(0, top);
  right = std::max(0, right);
  bottom = std::max(0, bottom);

  clip = make_crop(clip, left, top, vi.width - left - right, vi.height - top - bottom, false, env);
  return make_add_borders_with_transient(clip, left, top, right, bottom, color, color_as_yuv, args[7], args[8], args[9],
                                         args[10], args[11], env);
}

} // namespace aif::filters::crop
