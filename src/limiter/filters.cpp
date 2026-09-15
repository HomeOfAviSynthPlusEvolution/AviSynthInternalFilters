// SPDX-License-Identifier: GPL-2.0-or-later
#include "limiter/filters.h"
#include "limiter.h"
namespace aif::filters::limiter {
const std::array<Registration, 1>& registrations() {
  static const std::array<Registration, 1> functions = {{
      {"Limiter", "c[min_luma]f[max_luma]f[min_chroma]f[max_chroma]f[show]s[paramscale]b", Limiter::Create, nullptr},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::limiter
