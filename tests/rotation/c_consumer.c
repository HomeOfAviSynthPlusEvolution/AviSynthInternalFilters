// SPDX-License-Identifier: GPL-2.0-or-later
#include <rotation/kernel.h>
int main(void) {
  const uint8_t src[4] = {1, 2, 3, 4};
  uint8_t dst[4] = {0};
  if (aif_rotation_apply(src, dst, 2, 2, 2, 2, 1, AIF_ROTATION_RIGHT, 0))
    return 1;
  return !(dst[0] == 3 && dst[1] == 1 && dst[2] == 4 && dst[3] == 2);
}
