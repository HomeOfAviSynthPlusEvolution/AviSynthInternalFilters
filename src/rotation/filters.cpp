// SPDX-License-Identifier: GPL-2.0-or-later
#include "rotation/filters.h"
#include "turn.h"
#include "flip_horizontal.h"
#include "flip_vertical.h"
namespace aif::filters::rotation {
const std::array<Registration, 5>& registrations() {
  static const std::array<Registration, 5> functions = {{
      {"TurnLeft", "c", Turn::create_turnleft, nullptr},
      {"TurnRight", "c", Turn::create_turnright, nullptr},
      {"Turn180", "c", Turn::create_turn180, nullptr},
      {"FlipHorizontal", "c", FlipHorizontal::Create, nullptr},
      {"FlipVertical", "c", FlipVertical::Create, nullptr},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::rotation
