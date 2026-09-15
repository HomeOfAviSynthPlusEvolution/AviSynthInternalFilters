// SPDX-License-Identifier: GPL-2.0-or-later
#include "cpu_policy.h"
#include <cstdio>
#include "../../src/common/host_cpu.h"
namespace {
struct Host {
  bool v12;
  uint64_t flags;
  int checks = 0, legacy = 0, extended = 0;
  void CheckVersion(int version) {
    ++checks;
    if (version != 12 || !v12)
      throw AvisynthError("unsupported version");
  }
  int GetCPUFlags() {
    ++legacy;
    return static_cast<int>(flags);
  }
  int64_t GetCPUFlagsEx() {
    ++extended;
    return static_cast<int64_t>(flags);
  }
};
} // namespace
int main() {
  Host old{false, UINT64_C(0x80002000)}, modern{true, UINT64_C(1) << 40}, restricted{true, 0};
  if (aif::filters::host_cpu_flags(&old) != UINT64_C(0x80002000) || old.checks != 1 || old.legacy != 1 ||
      old.extended != 0 || aif::filters::host_cpu_flags(&modern) != (UINT64_C(1) << 40) || modern.checks != 1 ||
      modern.legacy != 0 || modern.extended != 1 || aif::filters::host_cpu_flags(&restricted) != 0 ||
      restricted.extended != 1) {
    std::fprintf(stderr, "host version fallback or per-environment CPU policy failed\n");
    return 1;
  }

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
