// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <internal_filters/registration.h>
namespace aif::filters::legacy_correction {
const std::array<Registration, 4>& registrations();
void register_filters(IScriptEnvironment* env);
} // namespace aif::filters::legacy_correction
