#include "backend.h"
#include <vector>
#include <limits>
#include <cstddef>
extern "C" int aif_rows_columns_process(const uint8_t* const src[], const int sp[], uint8_t* dst, int dp, int count,
                                        int h, int size, int period, int phase, int weave, uint32_t cpu) {
  if (!src || !sp || !dst || count <= 0 || h <= 0 || period <= 0 || (weave != 0 && weave != 1) ||
      (!weave && (phase < 0 || phase >= period)) ||
      (size != 0 && size != 1 && size != 2 && size != 3 && size != 4 && size != 6 && size != 8) ||
      (size == 0 && (count & 1)))
    return 1;
  int bytes = size == 0 ? 2 : size;
  if (count > std::numeric_limits<int>::max() / bytes / period)
    return 1;
  int alignment = size == 6 ? 2 : size == 3 || size == 0 ? 1 : size;
  if (dp % alignment || reinterpret_cast<uintptr_t>(dst) % alignment)
    return 1;
  for (int c = 0; c < (weave ? period : 1); ++c)
    if (sp[c] % alignment || reinterpret_cast<uintptr_t>(src[c]) % alignment)
      return 1;
  int row = count * bytes;
  if (dp < row * (weave ? period : 1))
    return 1;
  for (int c = 0; c < (weave ? period : 1); ++c)
    if (!src[c] || sp[c] < row * (weave ? 1 : period))
      return 1;
  try {
    std::vector<const uint8_t*> rows(weave ? period : 1);
    auto fn = aif::rows_columns::backend(cpu);
    if (!fn)
      fn = aif::rows_columns::scalar;
    for (int y = 0; y < h; ++y) {
      for (size_t c = 0; c < rows.size(); ++c)
        rows[c] = src[c] + ptrdiff_t(y) * sp[c];
      fn(rows.data(), dst + ptrdiff_t(y) * dp, count, size, period, phase, weave);
    }
    return 0;
  } catch (...) {
    return 2;
  }
}
