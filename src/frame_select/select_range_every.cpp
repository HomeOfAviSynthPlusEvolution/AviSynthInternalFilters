// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://avisynth.nl

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

#include "select_range_every.h"
#include <avs/minmax.h>
#include <algorithm>
#include <cstring>
namespace aif::filters::frame_select {
SelectRangeEvery::SelectRangeEvery(PClip _child, int _every, int _length, int _offset, bool _audio,
                                   IScriptEnvironment* env)
    : NonCachedGenericVideoFilter(_child), audio(_audio), achild(_child) {
  const int64_t num_audio_samples = vi.num_audio_samples;

  AVSValue trimargs[3] = {_child, _offset, 0};
  PClip c = env->Invoke("Trim", AVSValue(trimargs, 3)).AsClip();
  child = c;
  vi = c->GetVideoInfo();

  every = clamp(_every, 1, vi.num_frames);
  length = clamp(_length, 1, every);

  const int n = vi.num_frames;
  vi.num_frames = (n / every) * length + (n % every < length ? n % every : length);

  if (audio && vi.HasAudio()) {
    vi.num_audio_samples = vi.AudioSamplesFromFrames(vi.num_frames);
  } else {
    vi.num_audio_samples = num_audio_samples; // Undo Trim's work!
  }
}

PVideoFrame __stdcall SelectRangeEvery::GetFrame(int n, IScriptEnvironment* env) {
  return child->GetFrame((n / length) * every + (n % length), env);
}

bool __stdcall SelectRangeEvery::GetParity(int n) {
  return child->GetParity((n / length) * every + (n % length));
}

void __stdcall SelectRangeEvery::GetAudio(void* buf, int64_t start, int64_t count, IScriptEnvironment* env) {
  if (!audio) {
    // Use original unTrim'd child
    achild->GetAudio(buf, start, count, env);
    return;
  }

  int64_t samples_filled = 0;
  BYTE* samples = (BYTE*)buf;
  const int bps = vi.BytesPerAudioSample();
  int64_t position = start;
  int64_t block = vi.FramesFromAudioSamples(start) / length;
  auto boundary = [&](int64_t frame) {
    return vi.AudioSamplesFromFrames(int(std::min<int64_t>(frame, vi.num_frames)));
  };
  while (samples_filled < count) {
    while (boundary((block + 1) * length) <= position && (block + 1) * length < vi.num_frames)
      ++block;
    while (block > 0 && boundary(block * length) > position)
      --block;
    const int64_t begin = boundary(block * length);
    const int64_t end = boundary((block + 1) * length);
    const int64_t getsamples = std::min(end - position, count - samples_filled);
    if (getsamples <= 0) {
      // The host normally clips audio requests; keep the terminal tail silent.
      std::memset(samples + samples_filled * bps, vi.sample_type == SAMPLE_INT8 ? 128 : 0,
                  size_t(count - samples_filled) * bps);
      return;
    }
    const int64_t source_start = child->GetVideoInfo().AudioSamplesFromFrames(int(block * every)) + (position - begin);
    child->GetAudio(samples + samples_filled * bps, source_start, getsamples, env);
    samples_filled += getsamples;
    position += getsamples;
  }
}

AVSValue __cdecl SelectRangeEvery::Create(AVSValue args, void* user_data, IScriptEnvironment* env) {
  (void)user_data;
  return new SelectRangeEvery(args[0].AsClip(), args[1].AsInt(1500), args[2].AsInt(50), args[3].AsInt(0),
                              args[4].AsBool(true), env);
}

} // namespace aif::filters::frame_select
