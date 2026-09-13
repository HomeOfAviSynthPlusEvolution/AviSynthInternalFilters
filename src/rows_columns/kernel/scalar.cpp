#include "backend.h"
#include <cstring>
void aif::rows_columns::scalar(const uint8_t* const* s, uint8_t* d, int count, int size, int period, int phase,
                               int weave) {
  if (size == 0) {
    for (int m = weave ? 0 : phase; m < (weave ? period : phase + 1); ++m)
      for (int x = 0; x < count / 2; ++x) {
        int j = 4 * x, i = 2 * m + 4 * x * period;
        int wide[] = {i, i + 2 * m + 1, i + 2 * period, i + 2 * m + 3};
        for (int c = 0; c < 4; ++c)
          if (weave)
            d[wide[c]] = s[m][j + c];
          else
            d[j + c] = s[0][wide[c]];
      }
  } else if (weave) {
    for (int m = 0; m < period; ++m)
      for (int x = 0; x < count; ++x)
        std::memcpy(d + (x * period + m) * size, s[m] + x * size, size);
  } else
    for (int x = 0; x < count; ++x)
      std::memcpy(d + x * size, s[0] + (x * period + phase) * size, size);
}
