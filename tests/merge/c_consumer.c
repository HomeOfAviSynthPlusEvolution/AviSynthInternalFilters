#include "merge/kernel.h"
int main(void) {
  uint8_t b = 0, s = 100;
  aif_merge_plan* plan = 0;
  if (aif_merge_create(0, &plan))
    return 1;
  int status = aif_merge_mix_with_plan(plan, &b, &s, 1, 1, 1, 1, 8, 1, .5);
  aif_merge_destroy(plan);
  if (status || b != 50)
    return 2;
  return aif_merge_mix(&b, &s, 1, 1, 1, 1, 8, 1, .5, 0) || b != 75;
}
