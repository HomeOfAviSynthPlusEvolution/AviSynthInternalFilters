// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <avisynth.h>
namespace aif::filters::blank_clip {
PClip make_blank_clip(AVSValue, IScriptEnvironment*);
AVSValue __cdecl create_blank_clip(AVSValue, void*, IScriptEnvironment*);
} // namespace aif::filters::blank_clip
