// SPDX-License-Identifier: GPL-2.0-or-later
#include "crop/filters.h"
#include "crop.h"
#include "add_borders.h"
#include "letterbox.h"
namespace aif::filters::crop {
const std::array<Registration, 4>& registrations() {
  static const std::array<Registration, 4> functions = {{
      {"Crop", "ciiii[align]b", Crop::Create, nullptr},
      {"CropBottom", "ci", create_crop_bottom, nullptr},
      {"AddBorders", "ciiii[color]i[color_yuv]i[resample]s[param1]f[param2]f[param3]f[r]i", create_add_borders,
       nullptr},
      {"Letterbox", "cii[x1]i[x2]i[color]i[color_yuv]i[resample]s[param1]f[param2]f[param3]f[r]i", create_letterbox,
       nullptr},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::crop
