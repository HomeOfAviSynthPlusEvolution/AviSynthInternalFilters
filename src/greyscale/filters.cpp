// SPDX-License-Identifier: GPL-2.0-or-later
#include "greyscale/filters.h"
#include "greyscale.h"
namespace aif::filters::greyscale {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFGreyscale", "c[matrix]s", Greyscale::Create, nullptr);
  env->AddFunction("IFGrayscale", "c[matrix]s", Greyscale::Create, nullptr);
}
} // namespace aif::filters::greyscale
