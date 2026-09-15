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

#include "interleave.h"
#include <avs/minmax.h>
#include <algorithm>
namespace aif::filters::frame_select {
static int device_types(PClip clip) {
  int flags = clip->GetVersion() >= 5 ? clip->SetCacheHints(CACHE_GET_DEV_TYPE, 0) : 0;
  return flags ? flags : 1;
}
Interleave::Interleave(const std::vector<PClip>&& _child_array, IScriptEnvironment* env)
    : num_children((int)_child_array.size()), child_array(std::move(_child_array)) {
  vi = child_array[0]->GetVideoInfo();
  vi.MulDivFPS(num_children, 1);
  int64_t frame_count = int64_t(vi.num_frames - 1) * num_children + 1;
  child_devs = device_types(child_array[0]);
  for (int i = 1; i < num_children; ++i) {
    const VideoInfo& vi2 = child_array[i]->GetVideoInfo();
    if (vi.width != vi2.width || vi.height != vi2.height)
      env->ThrowError("Interleave: videos must be of the same size.");
    if (!vi.IsSameColorspace(vi2))
      env->ThrowError("Interleave: video formats don't match");

    frame_count = std::max(frame_count, int64_t(vi2.num_frames - 1) * num_children + i + 1);

    child_devs &= device_types(child_array[i]);
    if (child_devs == 0)
      env->ThrowError("Interleave: device types don't match");
  }
  if (frame_count > INT32_MAX || frame_count < 1)
    env->ThrowError("Interleave: Maximum number of frames exceeded.");
  vi.num_frames = int(frame_count);
}

int __stdcall Interleave::SetCacheHints(int cachehints, int frame_range) {
  (void)frame_range;
  switch (cachehints) {
    case CACHE_DONT_CACHE_ME:
      return 1;
    case CACHE_GET_MTMODE:
      return MT_NICE_FILTER;
    case CACHE_GET_DEV_TYPE:
      return child_devs;
    default:
      return 0;
  }
}

AVSValue __cdecl Interleave::Create(AVSValue args, void*, IScriptEnvironment* env) {
  args = args[0];
  const int num_args = args.ArraySize();
  if (num_args == 1)
    return args[0];

  std::vector<PClip> children(num_args);

  for (int i = 0; i < (int)children.size(); ++i)
    children[i] = args[i].AsClip();

  return new Interleave(std::move(children), env);
}
const VideoInfo& __stdcall Interleave::GetVideoInfo() {
  return vi;
}
PVideoFrame __stdcall Interleave::GetFrame(int n, IScriptEnvironment* env) {
  return child_array[congmod(n, num_children)]->GetFrame(n / num_children, env);
}
void __stdcall Interleave::GetAudio(void* buf, int64_t start, int64_t count, IScriptEnvironment* env) {
  child_array[0]->GetAudio(buf, start, count, env);
}
bool __stdcall Interleave::GetParity(int n) {
  return child_array[n % num_children]->GetParity(n / num_children);
}
} // namespace aif::filters::frame_select
