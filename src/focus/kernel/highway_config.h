// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#define HWY_WANT_SSE2 1
#define HWY_WANT_SSSE3 1
#define HWY_WANT_SSE4 1
#ifdef AIF_FOCUS_AVX2_ONLY
#define HWY_COMPILE_ONLY_STATIC
#else
#define HWY_COMPILE_ALL_ATTAINABLE
#endif
#define HWY_DISABLE_PCLMUL_AES
#define HWY_DISABLE_BMI2_FMA
#define HWY_DISABLE_F16C
// The public feature mask grants no AVX-512, crypto, FMA, or ARM extensions
// beyond NEON/SVE2. Keep compiled targets inside that instruction ceiling.
#if defined(AIF_FOCUS_SEPARATE_AVX2) && !defined(AIF_FOCUS_AVX2_ONLY)
#define AIF_HWY_AVX2_TARGET 0
#else
#define AIF_HWY_AVX2_TARGET HWY_AVX2
#endif
#define HWY_DISABLED_TARGETS                                                                                           \
  (~(HWY_SSE2 | HWY_SSSE3 | AIF_HWY_AVX2_TARGET | HWY_NEON_WITHOUT_AES | HWY_SVE2 | HWY_SCALAR | HWY_EMU128))
