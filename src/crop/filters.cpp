// SPDX-License-Identifier: GPL-2.0-or-later
#include "crop/filters.h"
#include "crop.h"
#include "add_borders.h"
#include "letterbox.h"
namespace aif::filters::crop {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFCrop", "ciiii[align]b", Crop::Create, nullptr);
  env->AddFunction("IFCropBottom", "ci", create_crop_bottom, nullptr);
  env->AddFunction("IFAddBorders", "ciiii[color]i[color_yuv]i[resample]s[param1]f[param2]f[param3]f[r]i",
                   create_add_borders, nullptr);
  env->AddFunction("IFLetterbox", "cii[x1]i[x2]i[color]i[color_yuv]i[resample]s[param1]f[param2]f[param3]f[r]i",
                   create_letterbox, nullptr);
}
} // namespace aif::filters::crop
