#include "crop/kernel.h"
int main(void) {
  unsigned char s = 7, d[3] = {0}, p = 2;
  return aif_crop_add_borders(&s, 1, 1, 1, d, 3, 3, 1, 1, 0, &p, 1, 0) || d[0] != 2 || d[1] != 7 || d[2] != 2;
}
