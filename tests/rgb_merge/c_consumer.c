#include "rgb_merge/kernel.h"
int main(void) {
  uint8_t s[] = {1, 2, 3, 4}, d[4] = {0};
  const uint8_t* src[] = {s, s, s, 0};
  int sp[] = {4, 4, 4, 0}, k[] = {4, 4, 4, 0};
  uint8_t* p[] = {d, 0, 0, 0};
  int dp[] = {4, 0, 0, 0};
  return aif_rgb_merge_render(src, sp, k, p, dp, 1, 1, 1, 4, 0) || d[0] != 1 || d[2] != 3 || d[3] != 0;
}
