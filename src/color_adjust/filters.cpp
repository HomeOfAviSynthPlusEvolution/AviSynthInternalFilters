// SPDX-License-Identifier: GPL-2.0-or-later
#include "color_adjust/filters.h"
#include "levels.h"
#include "rgb_adjust.h"
#include "tweak.h"
#include "color_yuv.h"
namespace aif::filters::color_adjust {
const std::array<Registration, 4>& registrations() {
  static const std::array<Registration, 4> functions = {{
      {"Levels", "cfffff[coring]b[dither]b", Levels::Create, nullptr},
      {"RGBAdjust",
       "c[r]f[g]f[b]f[a]f[rb]f[gb]f[bb]f[ab]f[rg]f[gg]f[bg]f[ag]f[analyze]b[dither]b[conditional]b[condvarsuffix]s",
       RGBAdjust::Create, nullptr},
      {"Tweak",
       "c[hue]f[sat]f[bright]f[cont]f[coring]b[sse]b[startHue]f[endHue]f[maxSat]f[minSat]f[interp]f[dither]"
       "b[realcalc]b[dither_strength]f",
       Tweak::Create, nullptr},
      {"ColorYUV",
       "c[gain_y]f[off_y]f[gamma_y]f[cont_y]f[gain_u]f[off_u]f[gamma_u]f[cont_u]f[gain_v]f[off_v]f[gamma_v]"
       "f[cont_v]f[levels]s[opt]s[matrix]s[showyuv]b[analyze]b[autowhite]b[autogain]b[conditional]b[bits]i["
       "showyuv_fullrange]b[f2c]b[condvarsuffix]s[optForceUseExpr]b",
       ColorYUV::Create, nullptr},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::color_adjust
