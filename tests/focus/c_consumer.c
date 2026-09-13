// SPDX-License-Identifier: GPL-2.0-or-later
#include "focus/kernel.h"
#include <stdio.h>
int main(void) {
  uint8_t source[4] = {0, 40, 80, 120};
  uint8_t destination[4] = {0};
  if (aif_focus_horizontal(source, 4, destination, 4, 4, 1, 8, AIF_FOCUS_PLANAR, 16384, 0.5f, 0) != AIF_FOCUS_OK)
    return 1;
  if (destination[0] != 10 || destination[1] != 40 || destination[2] != 80 || destination[3] != 110)
    return 2;
  printf("C consumer passed; supported CPU mask = %u\n", aif_focus_supported_cpu());
  return 0;
}
