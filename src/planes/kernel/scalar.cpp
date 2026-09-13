// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
void aif::planes::scalar(int op, const uint8_t* s, const uint8_t* u, const uint8_t* v, uint8_t* d, int count, int pos) {
  for (int x = 0; x < count; ++x) {
    if (op == 0) {
      d[4 * x] = s[4 * x];
      d[4 * x + 1] = s[4 * x + 3];
      d[4 * x + 2] = s[4 * x + 2];
      d[4 * x + 3] = s[4 * x + 1];
    } else if (op == 1)
      d[x] = s[4 * x + pos];
    else if (op == 2) {
      d[2 * x] = s[4 * x + pos];
      d[2 * x + 1] = 128;
    } else {
      d[4 * x] = s ? s[4 * x] : 126;
      d[4 * x + 1] = u[2 * x];
      d[4 * x + 2] = s ? s[4 * x + 2] : 126;
      d[4 * x + 3] = v[2 * x];
    }
  }
}
