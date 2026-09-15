#include <mask/kernel.h>
int main(void) {
  unsigned char d[4] = {1, 2, 3, 4};
  aif_mask_resolve(0)(d, 0, 1, 2, 255, 0);
  return d[3] != 255;
}
