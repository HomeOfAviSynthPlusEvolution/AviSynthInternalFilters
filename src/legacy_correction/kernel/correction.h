#pragma once
#include <cstdint>
#include <cstddef>
namespace aif::filters::legacy_correction {
void darken(uint8_t*, int, int, int, int, int);
void swap_chroma(uint8_t*, int, int, int);
void blend(uint8_t*, const uint8_t*, int, int, int, int, int);
void skew(uint8_t*, const uint8_t*, int, int, int, int, int);
} // namespace aif::filters::legacy_correction
