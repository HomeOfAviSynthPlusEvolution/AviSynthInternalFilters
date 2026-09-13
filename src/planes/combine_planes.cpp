// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from AviSynthPlus avs_core/filters/planeswap.cpp.
#include "combine_planes.h"
#include "kernel_adapter.h"
namespace aif::filters::planes {
AVSValue __cdecl CombinePlanes::CreateCombinePlanes(AVSValue args, void* user_data, IScriptEnvironment* env) {
  int mode = (int)(intptr_t)(user_data);
  int target_planes_param = 0 + mode;
  int source_planes_param = 1 + mode;
  int pixel_type_param = 2 + mode;
  int sample_clip_param = 3 + mode;

  bool hasSampleClip = args[sample_clip_param].Defined();

  return new CombinePlanes(args[0].AsClip(), mode >= 2 ? args[1].AsClip() : nullptr,
                           mode >= 3 ? args[2].AsClip() : nullptr, mode >= 4 ? args[3].AsClip() : nullptr,
                           hasSampleClip ? args[sample_clip_param].AsClip() : nullptr,
                           args[target_planes_param].AsString(""), args[source_planes_param].AsString(""),
                           args[pixel_type_param].AsString(""), env);
}

CombinePlanes::CombinePlanes(PClip _child, PClip _clip2, PClip _clip3, PClip _clip4, PClip _sample,
                             const char* _target_planes_str, const char* _source_planes_str, const char* _pixel_type,
                             IScriptEnvironment* env)
    : GenericVideoFilter(_child) {
  clips[0] = _child;
  clips[1] = _clip2;
  clips[2] = _clip3;
  clips[3] = _clip4;
  // planes(_planes), pixel_type(pixel_type)
  // getting target video format
  VideoInfo vi_default;
  memset(&vi_default, 0, sizeof(VideoInfo));

  bool videoFormatOverridden = false;

  if (_sample) {
    vi_default = _sample->GetVideoInfo();
    videoFormatOverridden = true;
  } else { // no sample video: format from first clip
    vi_default = child->GetVideoInfo();
  }
  // 1.) sample clip 2.) first clip 3.) pixel_type override
  // 4.) when input clips are greyscale, automatically use YUV(A)/RGB(A) depending on "planes" string
  if (*_pixel_type) {
    int i_pixel_type = pixel_type(_pixel_type, env);
    if (i_pixel_type == VideoInfo::CS_UNKNOWN)
      env->ThrowError("CombinePlanes: unknown pixel_type %s", _pixel_type);
    vi_default.pixel_type = i_pixel_type;
    videoFormatOverridden = true;
  }

  if (!vi_default.IsPlanar())
    env->ThrowError("CombinePlanes: output clip video format is not planar!");

  // autoconvert packed RGB or YUY2 inputs, in order to able to extract planes
  for (int i = 0; i < 4; i++) {
    if (!clips[i])
      continue;
    const VideoInfo& vi_test = clips[i]->GetVideoInfo();
    if (vi_test.IsRGB() && !vi_test.IsPlanar()) {
      bool hasAlpha = vi_test.NumComponents() == 4;
      clips[i] = convert(clips[i], hasAlpha ? "ConvertToPlanarRGBA" : "ConvertToPlanarRGB", env);
    } else if (vi_test.IsYUY2()) {
      AVSValue emptyValue;
      clips[i] = convert(clips[i], "ConvertToYV16", env);
    }
  }

  int source_plane_count = (int)strlen(_source_planes_str); // no check here, can be 0
  int target_plane_count = (int)strlen(_target_planes_str);
  if (target_plane_count == 0)
    env->ThrowError("CombinePlanes: no target planes given!");
  int clip_count = clips[3] ? 4 : clips[2] ? 3 : clips[1] ? 2 : 1;
  if (target_plane_count < clip_count)
    env->ThrowError("CombinePlanes: more clips specified than target planes");

  // If no video format was forced and no input planes were given
  // and all the source clips are Y, then
  // we give it a try of easy greyscale->RGB(A) or YUV(A) conversion
  // depending on the _target_planes_str
  bool allIsGrey = true;
  for (int i = 0; i < clip_count; i++) {
    if (!clips[i]->GetVideoInfo().IsY()) {
      allIsGrey = false;
      break;
    }
  }

  if (!videoFormatOverridden && source_plane_count == 0) {
    if (allIsGrey) {
      // special case. Figure out RGB(A) or YUV(A) or Y
      bool allIsYUV = true;
      bool allIsRGB = true;
      for (int i = 0; i < target_plane_count; i++) {
        char ch = toupper(_target_planes_str[i]);
        if (ch == 'R' || ch == 'G' || ch == 'B')
          allIsYUV = false;
        if (ch == 'Y' || ch == 'U' || ch == 'V')
          allIsRGB = false;
      }
      if (allIsYUV || allIsRGB) {
        int new_pixel_type;
        if (allIsRGB)
          new_pixel_type = target_plane_count == 4 ? VideoInfo::CS_GENERIC_RGBAP : VideoInfo::CS_GENERIC_RGBP;
        else // if (allIsYUV)
          new_pixel_type = target_plane_count == 4 ? VideoInfo::CS_GENERIC_YUVA444 : VideoInfo::CS_GENERIC_YUV444;
        int bits_mask = clips[0]->GetVideoInfo().pixel_type & VideoInfo::CS_Sample_Bits_Mask;
        new_pixel_type |= bits_mask; // copy bit-depth from the first clip
        vi_default.pixel_type = new_pixel_type;
      }
    }
  }

  vi = vi_default;

  if (!vi_default.IsPlanar())
    env->ThrowError("CombinePlanes: target format must be planar!");

  if (target_plane_count > vi_default.NumComponents())
    env->ThrowError("CombinePlanes: too many target planes (%d)! Target video plane count is %d!", target_plane_count,
                    vi_default.NumComponents());

  if (source_plane_count != 0 && source_plane_count != target_plane_count)
    env->ThrowError("CombinePlanes: source plane count must match with target plane count if provided!");

  // useful for later check
  bool targetIsYUV = vi_default.IsYUV() || vi_default.IsYUVA();
  bool targetHasAlpha = vi_default.IsYUVA() || vi_default.IsPlanarRGBA();
  bool targetIsY = vi_default.IsY();

  // class variables
  bits_per_pixel = vi_default.BitsPerComponent();
  pixelsize = vi_default.ComponentSize();
  planecount = target_plane_count;

  // if source plane is given, use it otherwise assume these
  const char* rgb_source_planes_str_def = "RGBA";
  const char* yuv_source_planes_str_def = allIsGrey ? "YYYY" : "YUVA";

  int last_clip_index = 0;
  for (int i = 0; i < target_plane_count; i++) {
    char ch = toupper(_target_planes_str[i]);
    bool isRGB = ch == 'R' || ch == 'G' || ch == 'B';
    bool isYUV = ch == 'Y' || ch == 'U' || ch == 'V';
    bool isAlpha = ch == 'A';
    if (!isRGB && !isYUV && !isAlpha)
      env->ThrowError("CombinePlanes: invalid plane definition :%s", _target_planes_str);
    if ((targetIsYUV && isRGB) || (!targetIsYUV && isYUV) || (!targetHasAlpha && isAlpha) || (targetIsY && ch != 'Y'))
      env->ThrowError("CombinePlanes: target has no such plane %c", ch);

    int current_target_plane;
    switch (ch) {
      case 'R':
        current_target_plane = PLANAR_R;
        break;
      case 'G':
        current_target_plane = PLANAR_G;
        break;
      case 'B':
        current_target_plane = PLANAR_B;
        break;
      case 'A':
        current_target_plane = PLANAR_A;
        break;
      case 'Y':
        current_target_plane = PLANAR_Y;
        break;
      case 'U':
        current_target_plane = PLANAR_U;
        break;
      case 'V':
        current_target_plane = PLANAR_V;
        break;
    }
    target_planes[i] = current_target_plane;
    int target_plane_width = vi_default.width >> vi_default.GetPlaneWidthSubsampling(current_target_plane);
    int target_plane_height = vi_default.height >> vi_default.GetPlaneHeightSubsampling(current_target_plane);

    if (clips[i])          // source clip count can be less than target planes count
      last_clip_index = i; // last defined clip is used for the others

    // check source clips and optinally their plane order
    VideoInfo src_vi = clips[last_clip_index]->GetVideoInfo();

    if (src_vi.BitsPerComponent() != bits_per_pixel)
      env->ThrowError("CombinePlanes: source bit depth is different from %d", bits_per_pixel);

    bool sourceIsYUV = src_vi.IsYUV() || src_vi.IsYUVA();
    bool sourceHasAlpha = src_vi.IsYUVA() || src_vi.IsPlanarRGBA();
    bool sourceIsY = src_vi.IsY();
    // check source
    // source_plane_count is either 0 or == target_plane_count
    {
      char ch;
      if (source_plane_count > 0) // optinal! defaults are filled
        ch = toupper(_source_planes_str[i]);
      else if (sourceIsYUV)
        ch = toupper(yuv_source_planes_str_def[i]);
      else // rgb
        ch = toupper(rgb_source_planes_str_def[i]);
      bool isRGB = ch == 'R' || ch == 'G' || ch == 'B';
      bool isYUV = ch == 'Y' || ch == 'U' || ch == 'V';
      bool isAlpha = ch == 'A';
      if (!isRGB && !isYUV && !isAlpha)
        env->ThrowError("CombinePlanes: invalid source plane definition :%s", _source_planes_str);
      if ((sourceIsYUV && isRGB) || (!sourceIsYUV && isYUV) || (!sourceHasAlpha && isAlpha) || (sourceIsY && ch != 'Y'))
        env->ThrowError("CombinePlanes: source has no such plane %c", ch);
      // todo lambda
      int current_source_plane;
      switch (ch) {
        case 'R':
          current_source_plane = PLANAR_R;
          break;
        case 'G':
          current_source_plane = PLANAR_G;
          break;
        case 'B':
          current_source_plane = PLANAR_B;
          break;
        case 'A':
          current_source_plane = PLANAR_A;
          break;
        case 'Y':
          current_source_plane = PLANAR_Y;
          break;
        case 'U':
          current_source_plane = PLANAR_U;
          break;
        case 'V':
          current_source_plane = PLANAR_V;
          break;
      }
      source_planes[i] = current_source_plane;
      // check dimensions
      int source_plane_width = src_vi.width >> src_vi.GetPlaneWidthSubsampling(current_source_plane);
      int source_plane_height = src_vi.height >> src_vi.GetPlaneHeightSubsampling(current_source_plane);
      if (source_plane_width != target_plane_width || source_plane_height != target_plane_height)
        env->ThrowError("CombinePlanes: source and target plane dimensions are different");
    }
  }
}

PVideoFrame __stdcall CombinePlanes::GetFrame(int n, IScriptEnvironment* env) {

  VideoInfo vi_src = clips[0]->GetVideoInfo();

  // check if fast Subframe magic can replace BitBlt
  if (!clips[1] && vi.NumComponents() <= vi_src.NumComponents()) // YUV<->RGB, YUVA<->RGBA YUV->Y
  {
    // we have only one clip, plane shuffle is valid if target has less plane that defined in source
    PVideoFrame src = clips[0]->GetFrame(n, env);

    int planes_y[4] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
    int planes_r[4] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
    int* planes = (vi_src.IsYUV() || vi_src.IsYUVA()) ? planes_y : planes_r;

    int Offsets[4];
    int Pitches[4], NewPitches[4];
    int RowSizes[4], NewRowSizes[4];

    int RelOffsets[4];

    for (int i = 0; i < vi_src.NumComponents(); i++) {
      Offsets[i] = src->GetOffset(planes[i]);
      Pitches[i] = NewPitches[i] = src->GetPitch(planes[i]);
      RowSizes[i] = NewRowSizes[i] = src->GetRowSize(planes[i]);
      RelOffsets[i] = 0;
    }

    for (int i = 0; i < planecount; i++) {
      int target_plane = target_planes[i];
      int source_plane = source_planes[i];
      int target_index, source_index;
      switch (target_plane) {
        case PLANAR_Y:
        case PLANAR_G:
          target_index = 0;
          break;
        case PLANAR_U:
        case PLANAR_B:
          target_index = 1;
          break;
        case PLANAR_V:
        case PLANAR_R:
          target_index = 2;
          break;
        case PLANAR_A:
          target_index = 3;
          break;
      }
      switch (source_plane) {
        case PLANAR_Y:
        case PLANAR_G:
          source_index = 0;
          break;
        case PLANAR_U:
        case PLANAR_B:
          source_index = 1;
          break;
        case PLANAR_V:
        case PLANAR_R:
          source_index = 2;
          break;
        case PLANAR_A:
          source_index = 3;
          break;
      }
      // !! if offsets would be size_t, be cautious when you subtract two unsigned size_t variables
      RelOffsets[target_index] = Offsets[source_index] - Offsets[target_index];
      NewPitches[target_index] = Pitches[source_index];
      NewRowSizes[target_index] = RowSizes[source_index];
      // Y            U           V          A
      // 10         1010        2010       3010     offsets
      // src: AUVY target: YVUA
      // 3010-10   2010-1010  1010-2010    10-3010
      //  3000     =+1000      =-1000      =-3000   reloffsets
      //  3010       2010        1010       10      new offsets inside
    }

    PVideoFrame dst;
    if (vi.NumComponents() == 4) {
      dst = env->SubframePlanarA(src, RelOffsets[0], NewPitches[0], NewRowSizes[0], src->GetHeight(), RelOffsets[1],
                                 RelOffsets[2], NewPitches[1], RelOffsets[3]);
    } else if (vi.NumComponents() == 3) {
      dst = env->SubframePlanar(src, RelOffsets[0], NewPitches[0], NewRowSizes[0], src->GetHeight(), RelOffsets[1],
                                RelOffsets[2], NewPitches[1]);
    } else {
      dst = env->Subframe(src, RelOffsets[0], NewPitches[0], NewRowSizes[0], src->GetHeight());
    }

    // RGB(A)<->YUV(A) color space conversion can't be caught by Subframe...()
    dst->AmendPixelType(vi.pixel_type);

    return dst;
  }

  // check if first clip could be used as the target clip
  PVideoFrame src = clips[0]->GetFrame(n, env);
  PVideoFrame src1 = clips[1] ? clips[1]->GetFrame(n, env) : nullptr;

  // case 1: when Y is kept from the original clip and other planes may be merged
  if (vi_src.IsSameColorspace(vi) && target_planes[0] == source_planes[0]) {
    // source (clip#0) has the same format as the target, and the first plane is the same
    // luma (Y) comes w/o BitBlt. Only U and V (and optionally A) is copied.
    if (src->IsWritable()) // we are the only one
    {
      src->AmendPixelType(vi.pixel_type);

      PVideoFrame src_other = nullptr;
      bool writeptr_obtained = false;

      for (int i = 1; i < planecount; i++) {
        int target_plane = target_planes[i];
        int source_plane = source_planes[i];

        if (clips[i]) { // source clips can be less than defined planes
          if (!writeptr_obtained) {
            src->GetWritePtr(PLANAR_Y); //Must be requested BUT only if we actually do something
            writeptr_obtained = true;
          }

          if (i == 1)
            src_other = src1; // already requested
          else
            src_other = clips[i]->GetFrame(n, env); // last defined clip is used for the others
        }

        if (src_other) {
          env->BitBlt(src->GetWritePtr(target_plane), src->GetPitch(target_plane), src_other->GetReadPtr(source_plane),
                      src_other->GetPitch(source_plane), src_other->GetRowSize(source_plane),
                      src_other->GetHeight(source_plane));
        } else {
          // we are still at the first (master) clip, no need for plane copy
        }
      }

      return src;
    }
  } else if (clips[1] && !clips[2]) {
    // Try to optimize a MergeLuma case, where luma comes from Y (can even be a format of single plane),
    // Clip a's UV is kept.
    // MergeLuma's speed gain: if 'a' is IsWritable() then there is no need for BitBlt chroma planes.
    // We can only make a BitBlt from Y.
    // We'd like to recognize the following scenario
    // Output YUV:
    // - Y from clip #0 (format:Y)
    // - UV from clip #1 (format YUV420, same as output)
    // planes: "YUV"
    //
    // clip #0 format does not match with the output, maybe it is a single plane
    // let's try with the second (clip #1) if it can be used
    if (clips[1]->GetVideoInfo().IsSameColorspace(vi) &&
        // the rest plane IDs are matching between source and target
        vi.NumComponents() >= 3 && target_planes[1] == source_planes[1] && target_planes[2] == source_planes[2] &&
        (vi.NumComponents() < 4 || (vi.NumComponents() == 4 && target_planes[3] == source_planes[3]))) {
      if (src1->IsWritable()) // we are the only one
      {
        src1->AmendPixelType(vi.pixel_type);

        src1->GetWritePtr(PLANAR_Y); //Must be requested BUT only if we actually do something

        int target_plane = target_planes[0];
        int source_plane = source_planes[0];

        // Copy from first clip
        env->BitBlt(src1->GetWritePtr(target_plane), src1->GetPitch(target_plane), src->GetReadPtr(source_plane),
                    src->GetPitch(source_plane), src->GetRowSize(source_plane), src->GetHeight(source_plane));

        env->copyFrameProps(src, src1);

        return src1;
      }
    }
  }

  PVideoFrame dst = env->NewVideoFrame(vi);
  bool propCopied = false;

  for (int i = 0; i < planecount; i++) {
    if (clips[i]) { // source clips can be less than defined planes
      if (i > 0) {  // clip #0 was already requested
        if (i == 1) // clip #1 was already requested
          src = src1;
        else
          src = clips[i]->GetFrame(n, env); // last defined clip is used for the others
      }

      if (!propCopied) {
        env->copyFrameProps(src, dst);
        propCopied = true;
      }
    }

    int target_plane = target_planes[i];
    int source_plane = source_planes[i];

    env->BitBlt(dst->GetWritePtr(target_plane), dst->GetPitch(target_plane), src->GetReadPtr(source_plane),
                src->GetPitch(source_plane), src->GetRowSize(source_plane), src->GetHeight(source_plane));
  }

  return dst;
}

} // namespace aif::filters::planes
