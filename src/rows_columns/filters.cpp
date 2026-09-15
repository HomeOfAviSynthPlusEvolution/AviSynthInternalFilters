#include "rows_columns/filters.h"
#include "separate_columns.h"
#include "weave_columns.h"
#include "separate_rows.h"
#include "weave_rows.h"
namespace aif::filters::rows_columns {
const std::array<Registration, 4>& registrations() {
  static const std::array<Registration, 4> functions = {{
      {"SeparateColumns", "ci", SeparateColumns::Create, nullptr},
      {"WeaveColumns", "ci", WeaveColumns::Create, nullptr},
      {"SeparateRows", "ci", SeparateRows::Create, nullptr},
      {"WeaveRows", "ci", WeaveRows::Create, nullptr},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::rows_columns
