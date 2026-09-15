#pragma once
#include <avisynth.h>
namespace aif::filters::field {
class NonCachedGenericVideoFilter : public GenericVideoFilter {
public:
  using GenericVideoFilter::GenericVideoFilter;
  int __stdcall SetCacheHints(int hint, int range) override {
    if (hint == CACHE_DONT_CACHE_ME)
      return 1;
    if (hint == CACHE_GET_MTMODE)
      return MT_NICE_FILTER;
    if (hint == CACHE_GET_DEV_TYPE)
      return child->GetVersion() >= 5 ? child->SetCacheHints(hint, 0) : 0;
    return GenericVideoFilter::SetCacheHints(hint, range);
  }
};
} // namespace aif::filters::field
