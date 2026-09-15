#include "layer/filters.h"
#include "layer.h"
#include "subtract.h"
namespace aif::filters::layer {
const std::array<Registration, 2>& registrations() {
  static const std::array<Registration, 2> r = {
      {{"Layer", "cc[op]s[level]i[x]i[y]i[threshold]i[use_chroma]b[opacity]f[placement]s", Layer::Create, nullptr},
       {"Subtract", "cc", Subtract::Create, nullptr}}};
  return r;
}
void register_filters(IScriptEnvironment* e) {
  register_plugin(e, registrations());
}
} // namespace aif::filters::layer
