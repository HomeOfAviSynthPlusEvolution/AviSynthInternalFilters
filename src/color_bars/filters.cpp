// SPDX-License-Identifier: GPL-2.0-or-later
#include "color_bars/filters.h"
#include "color_bars.h"
namespace aif::filters::color_bars {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFColorBars", "[width]i[height]i[pixel_type]s[staticframes]b", ColorBars::Create, nullptr);
  env->AddFunction("IFColorBarsHD", "[width]i[height]i[pixel_type]s[staticframes]b", ColorBars::Create,
                   reinterpret_cast<void*>(1));
}
} // namespace aif::filters::color_bars
