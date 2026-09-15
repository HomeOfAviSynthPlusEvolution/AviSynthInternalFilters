#include "overlay/filters.h"
#include "overlay.h"
namespace aif::filters::overlay {
const std::array<Registration, 1>& registrations() {
  static const std::array<Registration, 1> r = {{{"Overlay",
                                                  "cc[x]i[y]i[mask]c[opacity]f[mode]s[greymask]b[output]s[ignore_"
                                                  "conditional]b[PC_Range]b[use444]b[condvarsuffix]s",
                                                  Overlay::Create, nullptr}}};
  return r;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::overlay
