// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include <cstdint>

namespace aif::filters {
// Query at filter construction, not while rendering. Keep each environment's
// CPU restrictions; a process-wide cache would bypass SetMaxCPU settings.
template <class Environment>
inline uint64_t host_cpu_flags(Environment* env) {
  try {
    env->CheckVersion(12);
  } catch (const AvisynthError&) {
    // The old API returns an int bitmask. Zero-extend it, never sign-extend
    // unavailable high-bit capabilities into the v12 mask.
    return static_cast<uint32_t>(env->GetCPUFlags());
  }
  return static_cast<uint64_t>(env->GetCPUFlagsEx());
}
} // namespace aif::filters
