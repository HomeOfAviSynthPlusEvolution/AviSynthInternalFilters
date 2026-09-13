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

#include "general_convolution.h"
#include "kernel_adapter.h"
#include "convolution/kernel.h"
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <regex>
#include <string>
#include <iterator>
GeneralConvolution::GeneralConvolution(PClip _child, double _divisor, float _nBias, const char* _matrix,
                                       bool _autoscale, bool _luma, bool _chroma, bool _alpha, IScriptEnvironment* _env)
    : GenericVideoFilter(_child), divisor(_divisor), nBias(0), fBias(_nBias), autoscale(_autoscale), luma(_luma),
      chroma(_chroma), alpha(_alpha) {
  if (!std::isfinite(_divisor) || !std::isfinite(_nBias))
    _env->ThrowError("GeneralConvolution: divisor and bias must be finite");

  if (vi.BitsPerComponent() != 32) {
    if (double(_nBias) < double(INT32_MIN) || double(_nBias) > double(INT32_MAX))
      _env->ThrowError("GeneralConvolution: integer bias is out of range");
    nBias = int(_nBias);
  }
  if (vi.Is420() || vi.Is422() || vi.IsYV411()) {
    if (luma && chroma)
      _env->ThrowError("GeneralConvolution: both luma and chroma cannot be set for subsampled video formats");
  }
  if (!vi.IsRGB() && !vi.IsYUV() && !vi.IsYUVA())
    _env->ThrowError("GeneralConvolution requires RGB (planar or packed), greyscale or YUV(A) input");
  if (divisor == 0.0)
    _env->ThrowError("GeneralConvolution: divisor cannot be zero");
  setMatrix(_matrix, vi.BitsPerComponent() < 32, _env);

  if (vi.BitsPerComponent() <= 16) {
    // precompute divisor
    int64_t iCountT;
    if (autoscale) {
      iCountT = iNormalizeSum;
    } else {
      iCountT = 0;
    }

    // Truncate instead of round - keep in the spirit of the original code
    // 3.6.3: we do introduce rounding before scaling back from +20 bit range
    // 0x100000: 20 bit precision integer arithmetic
    const double factor = 0x100000 / (iCountT == 0 ? divisor : iCountT * divisor);
    if (!std::isfinite(factor) || factor < double(INT32_MIN) || factor > double(INT32_MAX))
      _env->ThrowError("GeneralConvolution: integer normalizing factor is out of range");
    iCountDiv = int(factor);
    if (iCountDiv == 0)
      _env->ThrowError("GeneralConvolution: normalizing factor is zero, check for too large elements or divisor value");

    // Guard the existing signed 64-bit arithmetic domain before rendering.
    uint64_t sum = uint64_t(iWeightSumPositives - iWeightSumNegatives);
    // Match the C kernel's bound, including stored high bits in 10/12/14-bit clips.
    uint64_t bound = sum * (vi.ComponentSize() == 1 ? 255u : 65535u);
    uint64_t factor_abs = iCountDiv < 0 ? uint64_t(-int64_t(iCountDiv)) : uint64_t(iCountDiv);
    if (bound > uint64_t(INT64_MAX - (1 << 19)) / factor_abs)
      _env->ThrowError("GeneralConvolution: integer matrix arithmetic exceeds 64-bit range");

  } else {
    // 32 bit float clip
    // precompute divisor
    float fCountT;
    if (autoscale) {
      fCountT = fNormalizeSum;
    } else {
      fCountT = 0.0f;
    }

    fCountDiv = (float)(1.0f / (fCountT == 0 ? divisor : fCountT * divisor));
  }
}

AVSValue __cdecl GeneralConvolution::Create(AVSValue args, void*, IScriptEnvironment* env) {
  const VideoInfo& vi_orig = args[0].AsClip()->GetVideoInfo();

  // convert old RGB format to planar RGB
  AVSValue new_args[1] = {args[0].AsClip()};
  PClip clip;
  if (vi_orig.IsRGB24() || vi_orig.IsRGB48()) {
    clip = env->Invoke("ConvertToPlanarRGB", AVSValue(new_args, 1)).AsClip();
  } else if (vi_orig.IsRGB32() || vi_orig.IsRGB64()) {
    clip = env->Invoke("ConvertToPlanarRGBA", AVSValue(new_args, 1)).AsClip();
  } else if (vi_orig.IsYUY2()) {
    clip = env->Invoke("ConvertToYV16", AVSValue(new_args, 1)).AsClip();
  } else {
    clip = args[0].AsClip();
  }

  GeneralConvolution* Result = new GeneralConvolution(
      clip, args[3].AsFloat(1.0f), args[1].AsFloatf(0.0f), args[2].AsString("0 0 0 0 1 0 0 0 0"), args[4].AsBool(true),
      args[5].AsBool(true), args[6].AsBool(true), args[7].AsBool(true), // luma, chroma, alpha, when n/a then ignored
      env);

  AVSValue new_args2[1] = {Result};
  if (vi_orig.IsRGB24()) {
    return env->Invoke("ConvertToRGB24", AVSValue(new_args2, 1)).AsClip();
  } else if (vi_orig.IsRGB48()) {
    return env->Invoke("ConvertToRGB48", AVSValue(new_args2, 1)).AsClip();
  } else if (vi_orig.IsRGB32()) {
    return env->Invoke("ConvertToRGB32", AVSValue(new_args2, 1)).AsClip();
  } else if (vi_orig.IsRGB64()) {
    return env->Invoke("ConvertToRGB64", AVSValue(new_args2, 1)).AsClip();
  } else if (vi_orig.IsYUY2()) {
    return env->Invoke("ConvertToYUY2", AVSValue(new_args2, 1)).AsClip();
  }

  return Result;
}

void GeneralConvolution::setMatrix(const char* _matrix, bool _isInteger, IScriptEnvironment* env) {
  char delimiter[] = "([ \t\n\r]+)";
  std::regex regex(delimiter);
  std::string str(_matrix);
  std::vector<std::string> out(std::sregex_token_iterator(str.begin(), str.end(), regex, -1),
                               std::sregex_token_iterator());

  fNormalizeSum = 0.0f;
  iNormalizeSum = 0;
  iWeightSumPositives = 0; // for int32 overflow decision
  iWeightSumNegatives = 0;
  const int MAX_DIMENSION = 9; // 9 is the max matrix size which is templated

  nSize = 0;
  int dim = 3;
  int maxsize = dim * dim;
  if (_isInteger)
    iMatrix.resize(maxsize);
  else
    fMatrix.resize(maxsize);
  for (auto& s : out) {
    if (s.length() == 0)
      continue; // first string can be empty is matrix string is starting with separators

    if (nSize == size_t(maxsize)) {
      if (dim == MAX_DIMENSION) {
        env->ThrowError("GeneralConvolution: matrix too big, maximum %dx%d elements allowed", MAX_DIMENSION,
                        MAX_DIMENSION);
      }
      dim += 2;
      maxsize = dim * dim;
      if (_isInteger)
        iMatrix.resize(maxsize);
      else
        fMatrix.resize(maxsize);
    }
    const double val = atof(s.c_str());
    if (!std::isfinite(val))
      env->ThrowError("GeneralConvolution: matrix elements must be finite");
    if (_isInteger) {
      const double rounded = std::round(val);
      if (rounded < double(INT32_MIN) || rounded > double(INT32_MAX))
        env->ThrowError("GeneralConvolution: integer matrix element is out of range");
      const int ival = int(rounded);
      iNormalizeSum += ival;
      iMatrix[nSize++] = ival;
      if (ival >= 0)
        iWeightSumPositives += ival;
      else
        iWeightSumNegatives += ival;
    } else {
      const float fval = static_cast<float>(val);
      if (!std::isfinite(fval))
        env->ThrowError("GeneralConvolution: float matrix element is out of range");
      fNormalizeSum += fval;
      fMatrix[nSize++] = fval;
    }
  }

  if (nSize < 9)
    env->ThrowError("GeneralConvolution: matrix too small, need at least 3x3 elements");
  else if (nSize != size_t(maxsize))
    env->ThrowError("GeneralConvolution: matrix incomplete, possible size %dx%d but element count %d", dim, dim,
                    int(nSize));
}

PVideoFrame __stdcall GeneralConvolution::GetFrame(int n, IScriptEnvironment* env) {
  int h = vi.height;
  int w = vi.width;

  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrameP(vi, &src);

  const int* matrix = iMatrix.data();
  const float* matrixf = fMatrix.data();

  int planes_y[4] = {PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
  int planes_r[4] = {PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
  int* planes = (vi.IsYUV() || vi.IsYUVA()) ? planes_y : planes_r;
  for (int p = 0; p < vi.NumComponents(); ++p) {
    const int plane = planes[p];
    if ((plane == PLANAR_Y && !luma) || ((plane == PLANAR_U || plane == PLANAR_V) && !chroma) ||
        (plane == PLANAR_A && !alpha)) {
      env->BitBlt(dst->GetWritePtr(plane), dst->GetPitch(plane), src->GetReadPtr(plane), src->GetPitch(plane),
                  src->GetRowSize(plane), src->GetHeight(plane));
      continue;
    }

    int width = w;
    int height = h;
    if (plane == PLANAR_U || plane == PLANAR_V) {
      width >>= vi.GetPlaneWidthSubsampling(plane);
      height >>= vi.GetPlaneHeightSubsampling(plane);
    }

    int dim = nSize == 9 ? 3 : nSize == 25 ? 5 : nSize == 49 ? 7 : 9;
    const uint32_t cpu = aif::filters::convolution::allowed_cpu(env);
    if (aif_convolution_apply(dst->GetWritePtr(plane), dst->GetPitch(plane), src->GetReadPtr(plane),
                              src->GetPitch(plane), width, height,
                              vi.BitsPerComponent() <= 16 ? static_cast<const void*>(matrix) : matrixf, dim,
                              vi.BitsPerComponent(), vi.BitsPerComponent() <= 16 ? iCountDiv : 0, nBias,
                              vi.BitsPerComponent() == 32 ? fCountDiv : 0, fBias, cpu))
      env->ThrowError("GeneralConvolution: unsupported kernel layout or arithmetic range");
  }
  return dst;
  // really, not other case left... packed RGB was converted to planar RGB, YUY2 to YV16
}
