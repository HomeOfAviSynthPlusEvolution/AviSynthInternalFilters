// Licensed under GPL version 2 or later with the inherited AviSynth linking exception.
#pragma once
#include <avisynth.h>
#include <composite/dispatch.h>

#include <algorithm>
#include <cmath>
namespace aif::filters::overlay {
inline cp_format Format(int bits) {
  return {bits == 8 ? CP_U8 : bits == 32 ? CP_F32 : CP_U16, bits};
}
inline cp_const_plane Read(cp_plane p) {
  return {p.data, p.stride, p.step};
}
inline void Check(int status, IScriptEnvironment* env) {
  if (status != CP_OK)
    env->ThrowError("Composite: invalid kernel arguments (%d)", status);
}
inline double Opacity(double value) {
  return std::isnan(value) ? 0.0 : std::clamp(value, 0.0, 1.0);
}
} // namespace aif::filters::overlay
