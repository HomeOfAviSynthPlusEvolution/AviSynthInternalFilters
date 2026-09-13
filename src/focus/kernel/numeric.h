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
namespace aif::focus::scalar {
[[maybe_unused]] static AVS_FORCEINLINE uint8_t ScaledPixelClip(int i) {
  // return PixelClip((i+32768) >> 16);
  // PF: clamp is faster than lut
  return (uint8_t)clamp((i + 32768) >> 16, 0, 255);
}

[[maybe_unused]] static AVS_FORCEINLINE uint16_t ScaledPixelClip(int64_t i) {
  return (uint16_t)clamp((i + 32768) >> 16, (int64_t)0, (int64_t)65535);
}

[[maybe_unused]] static AVS_FORCEINLINE uint16_t ScaledPixelClipEx(int64_t i, int max_value) {
  return (uint16_t)clamp((int)((i + 32768) >> 16), 0, max_value);
}

[[maybe_unused]] static AVS_FORCEINLINE bool IsClose(int a, int b, unsigned threshold) {
  return (unsigned(a - b + threshold) <= threshold * 2);
}

} // namespace aif::focus::scalar
