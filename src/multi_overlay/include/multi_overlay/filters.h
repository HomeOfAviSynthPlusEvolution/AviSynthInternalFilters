// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <internal_filters/registration.h>
namespace aif::filters::multi_overlay {
const std::array<Registration, 1>& registrations();
void register_filters(IScriptEnvironment* env);
} // namespace aif::filters::multi_overlay
