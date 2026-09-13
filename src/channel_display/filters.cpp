#include "channel_display/filters.h"
#include "show_channel.h"
namespace aif::filters::channel_display {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFShowAlpha", "c[pixel_type]s", ShowChannel::Create, (void*)3);
  env->AddFunction("IFShowRed", "c[pixel_type]s", ShowChannel::Create, (void*)2);
  env->AddFunction("IFShowGreen", "c[pixel_type]s", ShowChannel::Create, (void*)1);
  env->AddFunction("IFShowBlue", "c[pixel_type]s", ShowChannel::Create, (void*)0);
  env->AddFunction("IFShowY", "c[pixel_type]s", ShowChannel::Create, (void*)4);
  env->AddFunction("IFShowU", "c[pixel_type]s", ShowChannel::Create, (void*)5);
  env->AddFunction("IFShowV", "c[pixel_type]s", ShowChannel::Create, (void*)6);
}
} // namespace aif::filters::channel_display
