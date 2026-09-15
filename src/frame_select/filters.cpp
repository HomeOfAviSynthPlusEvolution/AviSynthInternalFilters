#include "frame_select/filters.h"
#include "interleave.h"
#include "select_every.h"
#include "select_range_every.h"
namespace aif::filters::frame_select {
const std::array<Registration, 5>& registrations() {
  static const std::array<Registration, 5> r = {
      {{"SelectEvery", "cii*", SelectEvery::Create, nullptr},
       {"SelectEven", "c", SelectEvery::Create_SelectEven, nullptr},
       {"SelectOdd", "c", SelectEvery::Create_SelectOdd, nullptr},
       {"Interleave", "c+", Interleave::Create, nullptr},
       {"SelectRangeEvery", "c[every]i[length]i[offset]i[audio]b", SelectRangeEvery::Create, nullptr}}};
  return r;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::frame_select
