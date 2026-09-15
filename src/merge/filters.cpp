#include "merge/filters.h"
#include "merge_all.h"
#include "merge_chroma.h"
#include "merge_luma.h"
namespace aif::filters::merge {
const std::array<Registration, 5>& registrations() {
  static const std::array<Registration, 5> functions = {{
      {"Merge", "cc[weight]f", MergeAll::Create, nullptr},
      {"MergeChroma", "cc[weight]f", MergeChroma::Create, nullptr},
      {"MergeChroma", "cc[chromaweight]f", MergeChroma::Create, nullptr},
      {"MergeLuma", "cc[weight]f", MergeLuma::Create, nullptr},
      {"MergeLuma", "cc[lumaweight]f", MergeLuma::Create, nullptr},
  }};
  return functions;
}
void register_filters(IScriptEnvironment* env) {
  register_plugin(env, registrations());
}
} // namespace aif::filters::merge
