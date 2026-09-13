#include "rgb_merge/filters.h"
#include "merge_rgb.h"
namespace aif::filters::rgb_merge {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFMergeRGB", "ccc[pixel_type]s", MergeRGB::Create, nullptr);
  env->AddFunction("IFMergeARGB", "cccc[pixel_type]s", MergeRGB::Create, (void*)1);
}
} // namespace aif::filters::rgb_merge
