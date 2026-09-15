#include "stack/filters.h"
#include "stack_vertical.h"
#include "stack_horizontal.h"
#include "show_five_versions.h"
namespace aif::filters::stack {
const std::array<Registration, 3>& registrations() {
  static const std::array<Registration, 3> functions = {{
      {"StackVertical", "cc+", StackVertical::Create, nullptr},
      {"StackHorizontal", "cc+", StackHorizontal::Create, nullptr},
      {"ShowFiveVersions", "ccccc", ShowFiveVersions::Create, nullptr},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::stack
