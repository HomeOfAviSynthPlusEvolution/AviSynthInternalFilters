// SPDX-License-Identifier: GPL-2.0-or-later
#include "planes/filters.h"
#include "swap_uv.h"
#include "swap_uv_to_y.h"
#include "swap_y_to_uv.h"
#include "combine_planes.h"
namespace aif::filters::planes {
const std::array<Registration, 20>& registrations() {
  static const std::array<Registration, 20> functions = {{
      {"SwapUV", "c", SwapUV::CreateSwapUV, nullptr},
      {"UToY", "c", SwapUVToY::CreateUToY, nullptr},
      {"VToY", "c", SwapUVToY::CreateVToY, nullptr},
      {"UToY8", "c", SwapUVToY::CreateUToY8, nullptr},
      {"VToY8", "c", SwapUVToY::CreateVToY8, nullptr},
      {"ExtractY", "c", SwapUVToY::CreateYToY8, nullptr},
      {"ExtractU", "c", SwapUVToY::CreateUToY8, nullptr},
      {"ExtractV", "c", SwapUVToY::CreateVToY8, nullptr},
      {"ExtractA", "c", SwapUVToY::CreateAnyToY8, (void*)SwapUVToY::AToY8},
      {"ExtractR", "c", SwapUVToY::CreateAnyToY8, (void*)SwapUVToY::RToY8},
      {"ExtractG", "c", SwapUVToY::CreateAnyToY8, (void*)SwapUVToY::GToY8},
      {"ExtractB", "c", SwapUVToY::CreateAnyToY8, (void*)SwapUVToY::BToY8},
      {"YToUV", "cc", SwapYToUV::CreateYToUV, nullptr},
      {"YToUV", "ccc", SwapYToUV::CreateYToYUV, nullptr},
      {"YToUV", "cccc", SwapYToUV::CreateYToYUVA, nullptr},
      {"PlaneToY", "c[plane]s", SwapUVToY::CreatePlaneToY8, nullptr},
      {"CombinePlanes", "c[planes]s[source_planes]s[pixel_type]s[sample_clip]c", CombinePlanes::CreateCombinePlanes,
       (void*)1},
      {"CombinePlanes", "cc[planes]s[source_planes]s[pixel_type]s[sample_clip]c", CombinePlanes::CreateCombinePlanes,
       (void*)2},
      {"CombinePlanes", "ccc[planes]s[source_planes]s[pixel_type]s[sample_clip]c", CombinePlanes::CreateCombinePlanes,
       (void*)3},
      {"CombinePlanes", "cccc[planes]s[source_planes]s[pixel_type]s[sample_clip]c", CombinePlanes::CreateCombinePlanes,
       (void*)4},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::planes
