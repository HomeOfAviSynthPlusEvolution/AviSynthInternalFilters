// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <internal_filters/registration.h>
namespace aif::filters::stack {
const std::array<Registration, 3>& registrations();
void register_filters(IScriptEnvironment* env);
} // namespace aif::filters::stack
