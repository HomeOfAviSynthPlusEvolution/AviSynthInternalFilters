#include "blank_clip/kernel.h"
int main(void) {
  uint8_t d[3] = {0}, p = 7;
  return aif_blank_clip_fill(d, 3, 3, 1, &p, 1, 0) || d[2] != 7;
}
