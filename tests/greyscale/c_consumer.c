#include "greyscale/kernel.h"
int main(void) {
  aif_greyscale_plan* p = 0;
  int s = aif_greyscale_create(.299, .114, 8, 1, 1, 0, &p);
  aif_greyscale_destroy(p);
  return s;
}
