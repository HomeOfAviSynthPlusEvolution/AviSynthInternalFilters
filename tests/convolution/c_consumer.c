#include "convolution/kernel.h"
int main(void) {
  return aif_convolution_apply(0, 0, 0, 0, 0, 0, 0, 3, 8, 1, 0, 0, 0, 0) == 1 ? 0 : 1;
}
