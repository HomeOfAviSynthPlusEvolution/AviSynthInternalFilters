// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#define HWY_WANT_SSE2 1
#define HWY_WANT_SSSE3 1
#define HWY_WANT_SSE4 1
#define HWY_WANT_AVX3_ZEN4 1
#define HWY_COMPILE_ALL_ATTAINABLE
// The transpose implementation stores vectors in arrays, which SVE's sizeless
// types cannot represent. Exclude SVE variants until that implementation supports
// them; ARM64 still builds the NEON kernels.
#define HWY_DISABLED_TARGETS (HWY_SVE | HWY_SVE2 | HWY_SVE_256 | HWY_SVE2_128)
