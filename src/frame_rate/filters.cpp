#include "frame_rate/filters.h"
#include "rational.h"
#include "assume_scaled_fps.h"
#include "assume_fps.h"
#include "change_fps.h"
#include "convert_fps.h"
namespace aif::filters::frame_rate {
const std::array<Registration, 15>& registrations() {
  static const std::array<Registration, 15> r = {
      {{"AssumeScaledFPS", "c[multiplier]i[divisor]i[sync_audio]b", AssumeScaledFPS::Create, nullptr},
       {"AssumeFPS", "ci[]i[sync_audio]b", AssumeFPS::Create, nullptr},
       {"AssumeFPS", "cf[sync_audio]b", AssumeFPS::CreateFloat, nullptr},
       {"AssumeFPS", "cs[sync_audio]b", AssumeFPS::CreatePreset, nullptr},
       {"AssumeFPS", "cc[sync_audio]b", AssumeFPS::CreateFromClip, nullptr},
       {"ChangeFPS", "ci[]i[linear]b", ChangeFPS::Create, nullptr},
       {"ChangeFPS", "cf[linear]b", ChangeFPS::CreateFloat, nullptr},
       {"ChangeFPS", "cs[linear]b", ChangeFPS::CreatePreset, nullptr},
       {"ChangeFPS", "cc[linear]b", ChangeFPS::CreateFromClip, nullptr},
       {"ConvertFPS", "ci[]i[zone]i[vbi]i", ConvertFPS::Create, nullptr},
       {"ConvertFPS", "cf[zone]i[vbi]i", ConvertFPS::CreateFloat, nullptr},
       {"ConvertFPS", "cs[zone]i[vbi]i", ConvertFPS::CreatePreset, nullptr},
       {"ConvertFPS", "cc[zone]i[vbi]i", ConvertFPS::CreateFromClip, nullptr},
       {"ContinuedDenominator", "f[]i[limit]i", ContinuedCreate, (void*)0},
       {"ContinuedNumerator", "f[]i[limit]i", ContinuedCreate, (void*)1}}};
  return r;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::frame_rate
