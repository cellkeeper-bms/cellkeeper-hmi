// What to send to the display: the rows that differ between the frame just drawn and the
// frame the display is showing. Most frames change a few numbers, so sending bands of rows
// instead of the whole frame keeps the panel update short.
#pragma once

#include <cstdint>

namespace hmiui {

struct Band {
  int y0, y1;  // rows y0 .. y1 - 1
};

// Finds the changed rows as bands, top to bottom. Bands closer together than `join` rows are
// sent as one. Returns how many bands were written to `out`; if there would be more than `max`,
// the last one runs to the last changed row.
int changed_bands(const uint16_t *drawn, const uint16_t *shown, int width, int height, Band *out, int max, int join = 12);

}  // namespace hmiui
