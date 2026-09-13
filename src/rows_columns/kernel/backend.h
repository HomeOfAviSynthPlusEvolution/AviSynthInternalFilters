#pragma once
#include "rows_columns/kernel.h"
namespace aif::rows_columns {
using Row = void (*)(const uint8_t* const*, uint8_t*, int, int, int, int, int);
void scalar(const uint8_t* const*, uint8_t*, int, int, int, int, int);
Row backend(uint32_t cpu);
} // namespace aif::rows_columns
