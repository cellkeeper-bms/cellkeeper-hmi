// What to send to the display: the rows that differ between the frame just drawn and the
// frame the display is showing. Most frames change a few numbers, so sending bands of rows
// instead of the whole frame keeps the panel update short.
#pragma once

#include <cstdint>

namespace hmiui {

struct Band {
  int y0, y1;  // rows y0 .. y1 - 1
};

// Finds the changed rows as bands, top to bottom, by comparing two frames. Bands closer
// together than `join` rows are sent as one. Returns how many bands were written to `out`; if
// there would be more than `max`, the last one runs to the last changed row.
int changed_bands(const uint16_t *drawn, const uint16_t *shown, int width, int height, Band *out, int max, int join = 12);

// The same without a second frame: `row_sums` holds one number per row of the frame the display
// shows (height entries, all zero before the first frame) and is brought up to date. Reading
// one frame instead of two halves the time, and the second frame's memory is not needed.
int changed_bands(const uint16_t *drawn, uint32_t *row_sums, int width, int height, Band *out, int max, int join = 12);

}  // namespace hmiui
