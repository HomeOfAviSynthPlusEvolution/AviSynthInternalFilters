// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
class IScriptEnvironment;
namespace aif::filters::focus {
// The embedding adapter/plugin initializes AVS_linkage from the host first.
// Prefix avoids replacing built-ins during migration and differential testing.
void register_filters(IScriptEnvironment* environment);
} // namespace aif::filters::focus
