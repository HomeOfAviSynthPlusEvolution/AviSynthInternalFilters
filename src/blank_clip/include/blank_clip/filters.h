// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <internal_filters/registration.h>
namespace aif::filters::blank_clip {
const std::array<Registration, 6>& registrations();
void register_filters(IScriptEnvironment* env);
} // namespace aif::filters::blank_clip
