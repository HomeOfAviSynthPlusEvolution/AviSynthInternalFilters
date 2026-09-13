#include <focus/kernel.h>
int main(void) {
  const unsigned char a = 1, b = 3;
  int64_t result = -1;
  if (aif_focus_selected_cpu(0) != 0)
    return 1;
  if (aif_focus_sad(&a, &b, 1, 1, 1, 1, 8, 0, &result) != AIF_FOCUS_OK || result != 2)
    return 2;
  return 0;
}
