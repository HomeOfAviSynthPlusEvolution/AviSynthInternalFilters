// SPDX-License-Identifier: GPL-2.0-or-later
#include "rotation/filters.h"
#include "turn.h"
#include "flip_horizontal.h"
#include "flip_vertical.h"
namespace aif::filters::rotation {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFTurnLeft", "c", Turn::create_turnleft, nullptr);
  env->AddFunction("IFTurnRight", "c", Turn::create_turnright, nullptr);
  env->AddFunction("IFTurn180", "c", Turn::create_turn180, nullptr);
  env->AddFunction("IFFlipHorizontal", "c", FlipHorizontal::Create, nullptr);
  env->AddFunction("IFFlipVertical", "c", FlipVertical::Create, nullptr);
}
} // namespace aif::filters::rotation
