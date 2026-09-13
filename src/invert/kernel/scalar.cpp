// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
#include <cstring>
namespace {
template <class T>
void integer(uint8_t* dst, int bytes, const uint8_t* pattern) {
  T mask;
  std::memcpy(&mask, pattern, sizeof(T));
  for (int x = 0; x < bytes; x += sizeof(T)) {
    T value;
    std::memcpy(&value, dst + x, sizeof(T));
    value ^= mask;
    std::memcpy(dst + x, &value, sizeof(T));
  }
}
} // namespace
void aif::invert::scalar(uint8_t* dst, int bytes, const uint8_t* pattern, int size, int mode) {
  if (mode >= 0) {
    for (int x = 0; x < bytes; x += 4) {
      float v;
      std::memcpy(&v, dst + x, 4);
      v = float(mode) - v;
      std::memcpy(dst + x, &v, 4);
    }
  } else {
    switch (size) {
      case 1:
        integer<uint8_t>(dst, bytes, pattern);
        break;
      case 2:
        integer<uint16_t>(dst, bytes, pattern);
        break;
      case 4:
        integer<uint32_t>(dst, bytes, pattern);
        break;
      case 8:
        integer<uint64_t>(dst, bytes, pattern);
        break;
      default:
        for (int x = 0; x < bytes; x += size)
          for (int c = 0; c < size; ++c)
            dst[x + c] ^= pattern[c];
    }
  }
}
