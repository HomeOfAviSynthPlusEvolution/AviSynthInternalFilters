#pragma once
#include <avisynth.h>
namespace aif::filters::field {
void copy_field(const PVideoFrame&, const PVideoFrame&, bool, bool, bool, IScriptEnvironment*);
void copy_alternate_lines(const PVideoFrame&, const PVideoFrame&, bool, bool, bool, IScriptEnvironment*);
} // namespace aif::filters::field
