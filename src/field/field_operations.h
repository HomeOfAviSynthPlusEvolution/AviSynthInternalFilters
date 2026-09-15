#pragma once
#include <avisynth.h>
namespace aif::filters::field {
AVSValue __cdecl Create_DoubleWeave(AVSValue, void*, IScriptEnvironment*);
AVSValue __cdecl Create_Weave(AVSValue, void*, IScriptEnvironment*);
AVSValue __cdecl Create_Pulldown(AVSValue, void*, IScriptEnvironment*);
AVSValue __cdecl Create_SwapFields(AVSValue, void*, IScriptEnvironment*);
AVSValue __cdecl Create_Bob(AVSValue, void*, IScriptEnvironment*);
} // namespace aif::filters::field
