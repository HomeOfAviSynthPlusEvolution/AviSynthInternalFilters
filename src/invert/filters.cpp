// SPDX-License-Identifier: GPL-2.0-or-later
#include "invert/filters.h"
#include "invert.h"
namespace aif::filters::invert {
const std::array<Registration, 1>& registrations() {
  static const std::array<Registration, 1> functions = {{
      {"Invert", "c[channels]s", Invert::Create, nullptr},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::invert
