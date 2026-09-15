// SPDX-License-Identifier: GPL-2.0-or-later
#include "greyscale/filters.h"
#include "greyscale.h"
namespace aif::filters::greyscale {
const std::array<Registration, 2>& registrations() {
  static const std::array<Registration, 2> functions = {{
      {"Greyscale", "c[matrix]s", Greyscale::Create, nullptr},
      {"Grayscale", "c[matrix]s", Greyscale::Create, nullptr},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::greyscale
