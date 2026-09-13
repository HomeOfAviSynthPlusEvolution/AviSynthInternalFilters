#include "planes/kernel.h"
int main(void) {
  uint8_t s[] = {1, 2, 3, 4}, d[4];
  return aif_planes_swap(s, 4, d, 4, 2, 1, 0) || d[1] != 4 || d[3] != 2;
}
