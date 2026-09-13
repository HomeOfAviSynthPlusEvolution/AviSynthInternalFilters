// SPDX-License-Identifier: GPL-2.0-or-later
#include "cpu_policy.h"
#include <cstdio>
int main() {
  using aif::filters::focus::host_cpu_mask;
  struct Case {
    int64_t flags;
    bool arm;
    uint32_t expected;
  };
  const Case cases[] = {{0, false, 0},
                        {0, true, 0},
                        {CPUF_FORCE, false, 0},
                        {CPUF_SSE2, false, AIF_FOCUS_SSE2},
                        {CPUF_SSSE3, false, AIF_FOCUS_SSSE3},
                        {CPUF_SSE4_1, false, AIF_FOCUS_SSE41},
                        {CPUF_AVX2, false, AIF_FOCUS_AVX2},
                        {CPUF_ARM_NEON, true, AIF_FOCUS_NEON},
                        {CPUF_ARM_SVE2, true, 0},
                        {CPUF_ARM_DOTPROD | CPUF_ARM_I8MM | CPUF_ARM_SVE2_1, true, 0},
                        {CPUF_ARM_NEON | CPUF_ARM_DOTPROD | CPUF_ARM_I8MM | CPUF_ARM_SVE2_1, true, AIF_FOCUS_NEON},
                        {CPUF_ARM_NEON | CPUF_ARM_SVE2, true, AIF_FOCUS_NEON | AIF_FOCUS_SVE2},
                        {CPUF_SSE2 | CPUF_SSSE3 | CPUF_SSE4_1 | CPUF_AVX2, false, 15},
                        {CPUF_SSE2 | CPUF_SSSE3 | CPUF_SSE4_1 | CPUF_AVX2, true, 0}};
  for (const auto& c : cases) {
    const auto actual = host_cpu_mask(c.flags, c.arm);
    if (actual != c.expected) {
      std::fprintf(stderr, "CPU flags %lld arm=%d: got %u expected %u\n", (long long)c.flags, c.arm, actual,
                   c.expected);
      return 1;
    }
  }
  std::puts("14 x86/ARM host CPU policy cases passed");
}
