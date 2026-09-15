#include "multi_overlay/filters.h"
#include "multi_overlay.h"
namespace aif::filters::multi_overlay {
const std::array<Registration, 1>& registrations() {
  static const std::array<Registration, 1> r = {{{"MultiOverlay", "cc+i+", MultiOverlay::Create, nullptr}}};
  return r;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::multi_overlay
