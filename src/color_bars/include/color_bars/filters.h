// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <internal_filters/registration.h>
namespace aif::filters::color_bars {
const std::array<Registration, 2>& registrations();
void register_filters(IScriptEnvironment* env);
} // namespace aif::filters::color_bars
