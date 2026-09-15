// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include <array>

namespace aif::filters {
struct Registration {
  const char* name;
  const char* arguments;
  AVSValue(__cdecl* create)(AVSValue, void*, IScriptEnvironment*);
  void* user_data;
};
template <size_t N>
inline void register_plugin(IScriptEnvironment* env, const std::array<Registration, N>& functions) {
  for (const auto& f : functions)
    env->AddFunction(f.name, f.arguments, f.create, f.user_data);
}
} // namespace aif::filters
