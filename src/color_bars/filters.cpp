// SPDX-License-Identifier: GPL-2.0-or-later
#include "color_bars/filters.h"
#include "color_bars.h"
namespace aif::filters::color_bars {
const std::array<Registration, 2>& registrations() {
  static const std::array<Registration, 2> functions = {{
      {"ColorBars", "[width]i[height]i[pixel_type]s[staticframes]b", ColorBars::Create, nullptr},
      {"ColorBarsHD", "[width]i[height]i[pixel_type]s[staticframes]b", ColorBars::Create, reinterpret_cast<void*>(1)},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::color_bars
