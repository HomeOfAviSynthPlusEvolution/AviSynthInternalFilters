#include "histogram/filters.h"
#include "histogram.h"
namespace aif::filters::histogram {
const std::array<Registration, 1>& registrations() {
  static const std::array<Registration, 1> r = {
      {{"Histogram", "c[mode]s[factor]f[bits]i[keepsource]b[markers]b", Histogram::Create, nullptr}}};
  return r;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::histogram
