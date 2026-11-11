#include "ui_present.h"

#include <cstring>

namespace hmiui {

// Adds a changed row to the bands: to the last one when it is close enough or there is no room
// for another, as a new one otherwise
static void add_row(Band *out, int &count, int max, int join, int y) {
  if (count > 0 && (y - out[count - 1].y1 < join || count == max)) {
    out[count - 1].y1 = y + 1;
  } else {
    out[count++] = {y, y + 1};
  }
}

int changed_bands(const uint16_t *drawn, const uint16_t *shown, int width, int height, Band *out, int max, int join) {
  int count = 0;
  const size_t row_bytes = (size_t) width * sizeof(uint16_t);
  for (int y = 0; y < height; y++) {
    if (memcmp(drawn + (size_t) y * width, shown + (size_t) y * width, row_bytes) != 0) add_row(out, count, max, join, y);
  }
  return count;
}

int changed_bands(const uint16_t *drawn, uint32_t *row_sums, int width, int height, Band *out, int max, int join) {
  int count = 0;
  for (int y = 0; y < height; y++) {
    // Two pixels at a time. Never zero, so that a row never matches "nothing shown yet".
    const uint16_t *row = drawn + (size_t) y * width;
    uint32_t sum = 2166136261u;
    int x = 0;
    for (; x + 1 < width; x += 2) sum = (sum ^ (row[x] | ((uint32_t) row[x + 1] << 16))) * 16777619u;
    if (x < width) sum = (sum ^ row[x]) * 16777619u;
    if (sum == 0) sum = 1;
    if (sum != row_sums[y]) {
      row_sums[y] = sum;
      add_row(out, count, max, join, y);
    }
  }
  return count;
}

}  // namespace hmiui
