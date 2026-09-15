// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
#include <limits>

namespace aif::filters {
// Equivalent to v11 propGetIntSaturated, using only the v8 property API.
inline int property_int(IScriptEnvironment* env, const AVSMap* map, const char* key, int index, int* error) {
  int status = 0;
  const int64_t value = env->propGetInt(map, key, index, &status);
  if (error)
    *error = status;
  if (status == 0) {
    if (value > std::numeric_limits<int>::max())
      return std::numeric_limits<int>::max();
    if (value < std::numeric_limits<int>::min())
      return std::numeric_limits<int>::min();
  }
  return static_cast<int>(value);
}
} // namespace aif::filters
