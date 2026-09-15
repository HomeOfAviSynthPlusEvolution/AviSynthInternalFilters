#include "chroma_scalar.h"
namespace aif::filters::overlay {
void chroma_scalar(uint8_t* d, const uint8_t* s, int dp, int sp, int w, int h, int bytes, bool expand, bool vertical) {
  if (bytes == 1)
    chroma_scalar_rows<uint8_t>(d, s, dp, sp, w, h, expand, vertical);
  else if (bytes == 2)
    chroma_scalar_rows<uint16_t>(d, s, dp, sp, w, h, expand, vertical);
  else
    chroma_scalar_rows<float>(d, s, dp, sp, w, h, expand, vertical);
}
} // namespace aif::filters::overlay
