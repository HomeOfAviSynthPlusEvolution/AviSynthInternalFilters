#pragma once
#include <avisynth.h>
#include <cstdint>
namespace aif::filters::frame_rate {
void ValidateFPS(const char*, uint64_t, uint64_t, IScriptEnvironment*);
void FrameRatio(const char*, unsigned, unsigned, unsigned, unsigned, int64_t&, int64_t&, IScriptEnvironment*);
int ScaleAudioRate(const char*, int, uint64_t, uint64_t, IScriptEnvironment*);
AVSValue __cdecl ContinuedCreate(AVSValue, void*, IScriptEnvironment*);
void FloatToFPS(const char*, float, unsigned&, unsigned&, IScriptEnvironment*);
void PresetToFPS(const char*, const char*, unsigned&, unsigned&, IScriptEnvironment*);
} // namespace aif::filters::frame_rate
