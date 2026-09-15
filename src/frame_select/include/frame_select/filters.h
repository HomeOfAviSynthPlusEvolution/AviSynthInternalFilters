// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <internal_filters/registration.h>
namespace aif::filters::frame_select {
const std::array<Registration, 5>& registrations();
void register_filters(IScriptEnvironment* env);
} // namespace aif::filters::frame_select
