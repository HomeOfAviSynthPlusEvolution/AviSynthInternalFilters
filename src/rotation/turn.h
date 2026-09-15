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

/*
** Turn. version 0.1
** (c) 2003 - Ernst Peché
**
*/

#pragma once
#include <avisynth.h>
#include <cstdint>
namespace aif::filters::rotation {
class Turn : public GenericVideoFilter {
  const uint32_t cpu_mask_;

  int operation;
  int bytes;
  PClip u_or_b_source;
  PClip v_or_r_source;

  int num_planes;
  int splanes[4];

  void SetUVSource(int mul_h, int mul_v, IScriptEnvironment* env);

public:
  Turn(PClip _child, int direction, IScriptEnvironment* env);

  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
  int __stdcall SetCacheHints(int cachehints, int frame_range) override;

  static AVSValue __cdecl create_turnleft(AVSValue args, void* user_data, IScriptEnvironment* env);
  static AVSValue __cdecl create_turnright(AVSValue args, void* user_data, IScriptEnvironment* env);
  static AVSValue __cdecl create_turn180(AVSValue args, void* user_data, IScriptEnvironment* env);
};

} // namespace aif::filters::rotation
