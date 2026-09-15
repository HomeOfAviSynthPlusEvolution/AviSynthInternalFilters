#pragma once
#include <cstdint>
#include <cstddef>
namespace aif::filters::overlay {
// width/height describe the smaller plane; expansion duplicates, reduction averages.
using Chroma = void (*)(uint8_t*, const uint8_t*, int, int, int, int, int, bool, bool);
Chroma select_chroma(int64_t allowed_targets);
void chroma_scalar(uint8_t*, const uint8_t*, int, int, int, int, int, bool, bool);
} // namespace aif::filters::overlay
