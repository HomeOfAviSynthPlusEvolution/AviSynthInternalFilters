#include "color_bars/kernel.h"
int main(void) {
  uint8_t d[64] = {0};
  uint8_t* p[4] = {d, 0, 0, 0};
  int pitch[4] = {16, 0, 0, 0};
  return aif_color_bars_draw(p, pitch, 4, 4, 8, AIF_COLOR_BARS_BGR32, 0, 0);
}
