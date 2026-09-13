// SPDX-License-Identifier: GPL-2.0-or-later
#include "single_frame.h"
#include <utility>
#include <cstring>
namespace aif::filters::blank_clip {
SingleFrame::SingleFrame(PClip child) : GenericVideoFilter(std::move(child)) {
}
PVideoFrame __stdcall SingleFrame::GetFrame(const int, IScriptEnvironment* env) {
  return child->GetFrame(0, env);
}
int __stdcall SingleFrame::SetCacheHints(const int cachehints, const int) {
  switch (cachehints) {
    case CACHE_DONT_CACHE_ME:
      return 1;
    case CACHE_GET_MTMODE:
      return MT_NICE_FILTER;
    case CACHE_GET_DEV_TYPE:
      return child->GetVersion() >= 5 ? child->SetCacheHints(CACHE_GET_DEV_TYPE, 0) : 0;
    default:
      return 0;
  }
}
} // namespace aif::filters::blank_clip
