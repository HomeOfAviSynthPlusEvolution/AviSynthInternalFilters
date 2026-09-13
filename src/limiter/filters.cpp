// SPDX-License-Identifier: GPL-2.0-or-later
#include "limiter/filters.h"
#include "limiter.h"
namespace aif::filters::limiter {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFLimiter", "c[min_luma]f[max_luma]f[min_chroma]f[max_chroma]f[show]s[paramscale]b",
                   Limiter::Create, nullptr);
}
} // namespace aif::filters::limiter
