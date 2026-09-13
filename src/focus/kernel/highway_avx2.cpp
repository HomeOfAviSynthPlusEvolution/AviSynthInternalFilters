// SPDX-License-Identifier: GPL-2.0-or-later
// MSVC needs /arch:AVX2 on the translation unit to keep YMM values in registers
// across loop edges. Isolating it also keeps dispatch and SSE paths at baseline.
#define AIF_FOCUS_AVX2_ONLY
#include "backend.h"
#include "highway_config.h"
#include <hwy/highway.h>
#include "highway_inl.h"
static_assert(HWY_TARGET == HWY_AVX2, "Compile this source with /arch:AVX2");
