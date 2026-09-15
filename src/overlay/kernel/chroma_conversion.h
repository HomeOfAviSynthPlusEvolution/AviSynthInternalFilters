#pragma once
#include <avisynth.h>
#include "chroma.h"
namespace aif::filters::overlay {
void Convert444FromYV12(PVideoFrame& src, PVideoFrame& dst, int bytes, int bits, IScriptEnvironment* env,
                        Chroma kernel);
void Convert444FromYV16(PVideoFrame& src, PVideoFrame& dst, int bytes, int bits, IScriptEnvironment* env,
                        Chroma kernel);
void Convert444ToYV12(PVideoFrame& src, PVideoFrame& dst, int bytes, int bits, IScriptEnvironment* env, Chroma kernel);
void Convert444ToYV16(PVideoFrame& src, PVideoFrame& dst, int bytes, int bits, IScriptEnvironment* env, Chroma kernel);
void ConvertYToYV12Chroma(BYTE* d, BYTE* s, int dp, int sp, int bytes, int w, int h, IScriptEnvironment* env,
                          Chroma kernel);
void ConvertYToYV16Chroma(BYTE* d, BYTE* s, int dp, int sp, int bytes, int w, int h, IScriptEnvironment* env,
                          Chroma kernel);
} // namespace aif::filters::overlay
