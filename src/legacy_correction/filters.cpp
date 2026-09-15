#include "legacy_correction/filters.h"
#include "fix_luminance.h"
#include "fix_broken_chroma_upsampling.h"
#include "peculiar_blend.h"
#include "skew_rows.h"
namespace aif::filters::legacy_correction {
const std::array<Registration, 4>& registrations() {
  static const std::array<Registration, 4> r = {
      {{"FixLuminance", "cif", FixLuminance::Create, nullptr},
       {"PeculiarBlend", "ci", PeculiarBlend::Create, nullptr},
       {"SkewRows", "ci", SkewRows::Create, nullptr},
       {"FixBrokenChromaUpsampling", "c", FixBrokenChromaUpsampling::Create, nullptr}}};
  return r;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::legacy_correction
