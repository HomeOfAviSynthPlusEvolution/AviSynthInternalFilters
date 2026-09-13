#include "channel_display/kernel.h"
int main(void) {
  uint8_t s[] = {1, 2, 3, 4}, d[4] = {0};
  uint8_t* p[] = {d, 0, 0, 0};
  int pitch[] = {4, 0, 0, 0};
  return aif_channel_display_render(s, 4, 0, 0, p, pitch, 1, 1, 1, 4, 4, 2, 0) || d[0] != 3 || d[3] != 4;
}
