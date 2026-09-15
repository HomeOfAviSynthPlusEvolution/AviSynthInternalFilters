// SPDX-License-Identifier: GPL-2.0-or-later
#include "mask/filters.h"
#include "mask.h"
#include "color_key_mask.h"
#include "reset_mask.h"
#include "mask_hs.h"
namespace aif::filters::mask {
const std::array<Registration, 4>& registrations() {
  static const std::array<Registration, 4> r = {
      {{"Mask", "cc", Mask::Create, nullptr},
       {"ColorKeyMask", "ci[]i[]i[]i", ColorKeyMask::Create, nullptr},
       {"ResetMask", "c[mask]f[opacity]f", ResetMask::Create, nullptr},
       {"MaskHS", "c[startHue]f[endHue]f[maxSat]f[minSat]f[coring]b[realcalc]b", MaskHS::Create, nullptr}}};
  return r;
}
void register_filters(IScriptEnvironment* e) {
  register_plugin(e, registrations());
}
} // namespace aif::filters::mask
