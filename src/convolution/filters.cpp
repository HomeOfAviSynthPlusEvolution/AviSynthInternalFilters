// SPDX-License-Identifier: GPL-2.0-or-later
#include "convolution/filters.h"
#include "general_convolution.h"
namespace aif::filters::convolution {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFGeneralConvolution", "c[bias]f[matrix]s[divisor]f[auto]b[luma]b[chroma]b[alpha]b",
                   GeneralConvolution::Create, nullptr);
}
} // namespace aif::filters::convolution
