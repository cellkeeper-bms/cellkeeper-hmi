# Plan: fix and refactor the hmi_bms component

**Status:** started 2026-10-25 on branch `hmi-bms-refactor`.
**Scope:** `components/hmi_bms/` (the ESPHome component that talks to the BMS over its HMI serial
link). The serial protocol on the wire stays the same; it is defined by the BMS firmware
(`bmstest1/bms/protocols/hmi_serial/`). **Every Home Assistant entity keeps its name and value**,
so dashboards and history carry on.

## What's wrong now

**Bugs:**
1. **Writes truncate instead of rounding.** `write_register()` sends `(int64_t)(value / scale)`.
   In float maths `98 / 0.01` can come out as 9799.9999, so a setting is saved one step low. There
   is no check against the register type's range either (a negative value into an unsigned
   register wraps).
2. **Events past the first page read as inactive.** A BMS reply carries at most 11 events
   (`tx_idx + 22 > 250` in `hmi_handle_read_events`) and returns `next_index` for the rest. The HMI
   always asks from index 0, ignores `next_index`, then sets every event type it didn't see to
   `INACTIVE`. A pack whose event history has more than 11 types loses events and counts in HA and
   on the LCD.
3. **A byte-swapped CRC is accepted as valid.** The BMS only sends the low byte first
   (`duart_send_packet`), so the fallback only doubles the chance of accepting a corrupted frame.
4. **Cell voltage delta depends on the YAML.** When a reply lacks min or max, the code reads the
   other from the sensor state and multiplies by 1000, assuming the YAML has `multiply: 0.001`.

**Structure:**
- Each register is described in three places that must agree: the poll lists in `update()`, a
  ~300-line if/else chain in `handle_read_registers_response_()` (the same byte decoding written
  out ~60 times), and the Python schema with ~80 setters and member pointers in the header.
- Requests were split by hand after replies overflowed the BMS's 256-byte buffer.
- The receive buffer is a `std::vector` that drops bytes from its front one at a time.
- Hidden `static` state in functions (address claiming, raw dump buffer).
- The settings lock and its status text (UI behaviour) live in the protocol class.
- Nothing is tested off the device.

**Fine as it is:** the slot-based poll schedule, the write path with readback through
`publish_from_bus`, deferring sensor publishes to `loop()`, the log flow control.

## Steps (each one built, flashed to the desk HMI and checked before the next)

1. **Bug fixes** (no structural change):
   - round writes with `lround` and clamp to the register type's range
   - page through events with `next_index`; mark types inactive only after the last page
   - drop the byte-swapped CRC fallback
   - keep the last raw cell min/max in mV in the component for the delta
2. **A protocol core without ESPHome**, `hmi_protocol.h/.cpp`: the CRC, a frame parser (an index
   into a fixed buffer instead of erasing from a vector), register value decoding by type, the
   delta-array decoding (cells, balancing times) and event records. `HMIBMS` calls into it.
   **Host tests** in `tests/` built with g++ in WSL: CRC, framing (split frames, junk between
   frames, bad CRC, oversize), every register type, the delta arrays and event pages, using frames
   in the BMS's own format.
3. **A register table.** One entry per register: id, wire type, scale, target (sensor or number),
   poll group (fast, slow, limits, settings, module temps). `update()` builds requests from the
   table and splits them by the reply size each will produce, so no reply passes the BMS's
   256-byte buffer. The parse chain becomes one lookup plus a few special cases (serial, board
   revision, firmware build, hard limits, SoC number).
4. **Python side generates the table.** `sensor.py` / `number.py` describe each register once
   (key, register, type, scale, unit, poll group) and register it with the hub
   (`add_register_sensor(reg, type, scale, group, sensor)`). The per-sensor setters and members go.
   YAML keys and entity names stay the same.
5. **Split the files:** `hmi_protocol.*` (step 2), `hmi_bms.cpp` (polling, dispatch, publishing),
   `hmi_events.cpp`, `hmi_log.cpp`, `hmi_balancing.cpp`, and the settings lock in its own small
   class.

**Checks at every step:** `esphome config` and a build for the desk Waveshare and Guition, the
host tests, then flash the desk Guition (it has the BMS link) and compare every HA entity with the
previous build: values, units, update rate. Settings: write each one from HA and the LCD and read
it back. Events: confirm all event types and counts match the BMS console.
