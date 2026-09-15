#include "field/filters.h"
#include "complement_parity.h"
#include "assume_parity.h"
#include "assume_field_based.h"
#include "assume_frame_based.h"
#include "separate_fields.h"
#include "double_weave_fields.h"
#include "double_weave_frames.h"
#include "fieldwise.h"
#include "field_operations.h"
namespace aif::filters::field {
const std::array<Registration, 11>& registrations() {
  static const std::array<Registration, 11> r = {{{"ComplementParity", "c", ComplementParity::Create, nullptr},
                                                  {"AssumeTFF", "c", AssumeParity::Create, (void*)true},
                                                  {"AssumeBFF", "c", AssumeParity::Create, (void*)false},
                                                  {"AssumeFieldBased", "c", AssumeFieldBased::Create, nullptr},
                                                  {"AssumeFrameBased", "c", AssumeFrameBased::Create, nullptr},
                                                  {"SeparateFields", "c", SeparateFields::Create, nullptr},
                                                  {"Weave", "c", Create_Weave, nullptr},
                                                  {"DoubleWeave", "c", Create_DoubleWeave, nullptr},
                                                  {"Pulldown", "cii", Create_Pulldown, nullptr},
                                                  {"SwapFields", "c", Create_SwapFields, nullptr},
                                                  {"Bob", "c[b]f[c]f[height]i", Create_Bob, nullptr}}};
  return r;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::field
