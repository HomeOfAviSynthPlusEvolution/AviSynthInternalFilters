// SPDX-License-Identifier: GPL-2.0-or-later
#include "planes/filters.h"
#include "swap_uv.h"
#include "swap_uv_to_y.h"
#include "swap_y_to_uv.h"
#include "combine_planes.h"
namespace aif::filters::planes {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFSwapUV", "c", SwapUV::CreateSwapUV, nullptr);
  env->AddFunction("IFUToY", "c", SwapUVToY::CreateUToY, nullptr);
  env->AddFunction("IFVToY", "c", SwapUVToY::CreateVToY, nullptr);
  env->AddFunction("IFUToY8", "c", SwapUVToY::CreateUToY8, nullptr);
  env->AddFunction("IFVToY8", "c", SwapUVToY::CreateVToY8, nullptr);
  env->AddFunction("IFExtractY", "c", SwapUVToY::CreateYToY8, nullptr);
  env->AddFunction("IFExtractU", "c", SwapUVToY::CreateUToY8, nullptr);
  env->AddFunction("IFExtractV", "c", SwapUVToY::CreateVToY8, nullptr);
  env->AddFunction("IFExtractA", "c", SwapUVToY::CreateAnyToY8, (void*)SwapUVToY::AToY8);
  env->AddFunction("IFExtractR", "c", SwapUVToY::CreateAnyToY8, (void*)SwapUVToY::RToY8);
  env->AddFunction("IFExtractG", "c", SwapUVToY::CreateAnyToY8, (void*)SwapUVToY::GToY8);
  env->AddFunction("IFExtractB", "c", SwapUVToY::CreateAnyToY8, (void*)SwapUVToY::BToY8);
  env->AddFunction("IFYToUV", "cc", SwapYToUV::CreateYToUV, nullptr);
  env->AddFunction("IFYToUV", "ccc", SwapYToUV::CreateYToYUV, nullptr);
  env->AddFunction("IFYToUV", "cccc", SwapYToUV::CreateYToYUVA, nullptr);
  env->AddFunction("IFPlaneToY", "c[plane]s", SwapUVToY::CreatePlaneToY8, nullptr);
  env->AddFunction("IFCombinePlanes", "c[planes]s[source_planes]s[pixel_type]s[sample_clip]c",
                   CombinePlanes::CreateCombinePlanes, (void*)1);
  env->AddFunction("IFCombinePlanes", "cc[planes]s[source_planes]s[pixel_type]s[sample_clip]c",
                   CombinePlanes::CreateCombinePlanes, (void*)2);
  env->AddFunction("IFCombinePlanes", "ccc[planes]s[source_planes]s[pixel_type]s[sample_clip]c",
                   CombinePlanes::CreateCombinePlanes, (void*)3);
  env->AddFunction("IFCombinePlanes", "cccc[planes]s[source_planes]s[pixel_type]s[sample_clip]c",
                   CombinePlanes::CreateCombinePlanes, (void*)4);
}
} // namespace aif::filters::planes
