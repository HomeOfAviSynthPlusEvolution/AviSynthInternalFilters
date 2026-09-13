// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "focus/kernel.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

#if defined(_MSC_VER)
#define AVS_FORCEINLINE __forceinline
#else
#define AVS_FORCEINLINE inline __attribute__((always_inline))
#endif
#if defined(__clang__)
#define CLANG 1
#elif defined(__GNUC__)
#define GCC 1
#endif

namespace aif::focus::scalar {
using BYTE = uint8_t;
using std::clamp;
using std::max;
using std::min;
} // namespace aif::focus::scalar
#include "numeric.h"
