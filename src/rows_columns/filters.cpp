#include "rows_columns/filters.h"
#include "separate_columns.h"
#include "weave_columns.h"
#include "separate_rows.h"
#include "weave_rows.h"
namespace aif::filters::rows_columns {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFSeparateColumns", "ci", SeparateColumns::Create, nullptr);
  env->AddFunction("IFWeaveColumns", "ci", WeaveColumns::Create, nullptr);
  env->AddFunction("IFSeparateRows", "ci", SeparateRows::Create, nullptr);
  env->AddFunction("IFWeaveRows", "ci", WeaveRows::Create, nullptr);
}
} // namespace aif::filters::rows_columns
