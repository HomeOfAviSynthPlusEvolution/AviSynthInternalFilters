#include "rgb_merge/filters.h"
#include "merge_rgb.h"
namespace aif::filters::rgb_merge {
const std::array<Registration, 2>& registrations() {
  static const std::array<Registration, 2> functions = {{
      {"MergeRGB", "ccc[pixel_type]s", MergeRGB::Create, nullptr},
      {"MergeARGB", "cccc[pixel_type]s", MergeRGB::Create, (void*)1},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::rgb_merge
