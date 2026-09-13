#include "merge/kernel.h"
int main(void) {
  uint8_t b = 0, s = 100;
  return aif_merge_mix(&b, &s, 1, 1, 1, 1, 8, 1, .5, 0) || b != 50;
}
