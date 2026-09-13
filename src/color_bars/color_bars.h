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

#pragma once
#include <avisynth.h>
namespace aif::filters::color_bars {
class ColorBars : public IClip {
  VideoInfo vi;
  PVideoFrame frame;
  float* audio;
  unsigned nsamples;
  bool
      staticframes; // P.F. false: a bit better for synthetic tests. Still defaults to true (one static frame is served)

  enum { Hz = 440 };

public:
  ~ColorBars();

  ColorBars(int w, int h, const char* pixel_type, bool _staticframes, int type, IScriptEnvironment* env);

  // By the new "staticframes" parameter: colorbars we generate (copy) real new frames instead of a ready-to-use static one
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  bool __stdcall GetParity(int n);
  const VideoInfo& __stdcall GetVideoInfo();
  int __stdcall SetCacheHints(int cachehints, int frame_range);

  void FillAudioZeros(void* buf, int start_offset, int count);

  void __stdcall GetAudio(void* buf, int64_t start, int64_t count, IScriptEnvironment* env);

  static AVSValue __cdecl Create(AVSValue args, void* _type, IScriptEnvironment* env);
};
} // namespace aif::filters::color_bars
