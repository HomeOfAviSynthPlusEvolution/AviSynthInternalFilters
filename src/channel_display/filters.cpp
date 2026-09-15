#include "channel_display/filters.h"
#include "show_channel.h"
namespace aif::filters::channel_display {
const std::array<Registration, 7>& registrations() {
  static const std::array<Registration, 7> functions = {{
      {"ShowAlpha", "c[pixel_type]s", ShowChannel::Create, (void*)3},
      {"ShowRed", "c[pixel_type]s", ShowChannel::Create, (void*)2},
      {"ShowGreen", "c[pixel_type]s", ShowChannel::Create, (void*)1},
      {"ShowBlue", "c[pixel_type]s", ShowChannel::Create, (void*)0},
      {"ShowY", "c[pixel_type]s", ShowChannel::Create, (void*)4},
      {"ShowU", "c[pixel_type]s", ShowChannel::Create, (void*)5},
      {"ShowV", "c[pixel_type]s", ShowChannel::Create, (void*)6},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::channel_display
