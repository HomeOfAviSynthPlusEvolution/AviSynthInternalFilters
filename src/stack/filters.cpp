#include "stack/filters.h"
#include "stack_vertical.h"
#include "stack_horizontal.h"
#include "show_five_versions.h"
namespace aif::filters::stack {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFStackVertical", "cc+", StackVertical::Create, nullptr);
  env->AddFunction("IFStackHorizontal", "cc+", StackHorizontal::Create, nullptr);
  env->AddFunction("IFShowFiveVersions", "ccccc", ShowFiveVersions::Create, nullptr);
}
} // namespace aif::filters::stack
