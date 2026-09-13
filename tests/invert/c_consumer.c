#include "invert/kernel.h"
int main(void) {
  uint8_t v = 0, mask = 255;
  return aif_invert_apply(&v, 1, 1, 1, &mask, 1, -1, 0) || v != 255;
}
