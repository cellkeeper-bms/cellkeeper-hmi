#include "ui_present.h"

#include <cstring>

namespace hmiui {

int changed_bands(const uint16_t *drawn, const uint16_t *shown, int width, int height, Band *out, int max, int join) {
  int count = 0;
  const size_t row_bytes = (size_t) width * sizeof(uint16_t);
  for (int y = 0; y < height; y++) {
    if (memcmp(drawn + (size_t) y * width, shown + (size_t) y * width, row_bytes) == 0) continue;
    if (count > 0 && (y - out[count - 1].y1 < join || count == max)) {
      // Close enough to the band before it, or no room for another band: that band grows
      out[count - 1].y1 = y + 1;
    } else {
      out[count++] = {y, y + 1};
    }
  }
  return count;
}

}  // namespace hmiui
