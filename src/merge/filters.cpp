#include "merge/filters.h"
#include "merge_all.h"
#include "merge_chroma.h"
#include "merge_luma.h"
namespace aif::filters::merge {
void register_filters(IScriptEnvironment* env) {
  env->AddFunction("IFMerge", "cc[weight]f", MergeAll::Create, nullptr);
  env->AddFunction("IFMergeChroma", "cc[weight]f", MergeChroma::Create, nullptr);
  env->AddFunction("IFMergeChroma", "cc[chromaweight]f", MergeChroma::Create, nullptr);
  env->AddFunction("IFMergeLuma", "cc[weight]f", MergeLuma::Create, nullptr);
  env->AddFunction("IFMergeLuma", "cc[lumaweight]f", MergeLuma::Create, nullptr);
}
} // namespace aif::filters::merge
