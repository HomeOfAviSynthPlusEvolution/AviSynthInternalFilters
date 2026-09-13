#include "rows_columns/kernel.h"
int main(void) {
  uint8_t s[] = {1, 2, 3, 4}, d[2] = {0};
  const uint8_t* p[] = {s};
  int sp[] = {4};
  return aif_rows_columns_process(p, sp, d, 2, 2, 1, 1, 2, 1, 0, 0) || d[0] != 2 || d[1] != 4;
}
