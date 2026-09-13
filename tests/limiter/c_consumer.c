#include "limiter/kernel.h"
int main(void) {
  uint8_t v = 0;
  aif_limiter_limits l = {16, 235, 16, 240, 8};
  return aif_limiter_apply(&v, 1, 1, 1, &l, 0, 0, 0) || v != 16;
}
