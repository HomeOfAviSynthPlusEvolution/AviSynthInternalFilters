// SPDX-License-Identifier: GPL-2.0-or-later
#include "invert/filters.h"
#include "invert.h"
namespace aif::filters::invert {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFInvert", "c[channels]s", Invert::Create, nullptr);
}
} // namespace aif::filters::invert
