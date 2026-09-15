// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend.h"
extern "C" aif_mask_row aif_mask_resolve(uint32_t cpu) {
  auto fn = aif::mask::backend(cpu);
  return fn ? fn : aif::mask::scalar;
}
