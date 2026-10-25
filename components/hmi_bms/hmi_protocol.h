// The BMS's HMI serial protocol, without ESPHome: framing, CRC, value and array decoding,
// event records. The BMS side is bmstest1/bms/protocols/hmi_serial/ and drivers/comms/duart.c.
// Host-tested by tests/hmi_protocol_test.cpp.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace hmi_bms {
namespace protocol {

// Frame on the wire: 0xFF, payload length - 1, payload (1..256 bytes), CRC16 low byte, high byte.
// The CRC (Modbus) covers the 0xFF and length bytes as well as the payload.
static const uint8_t SYNC = 0xFF;
static const size_t MAX_PAYLOAD = 256;
static const size_t FRAME_OVERHEAD = 4;

uint16_t crc16_modbus(const uint8_t *data, size_t len);

// Frame bytes for a payload (empty when the payload is empty or longer than MAX_PAYLOAD)
std::vector<uint8_t> build_frame(const uint8_t *payload, size_t len);
inline std::vector<uint8_t> build_frame(const std::vector<uint8_t> &payload) {
  return build_frame(payload.data(), payload.size());
}

// Collects received bytes and hands out complete, CRC-checked frames. After a bad CRC it drops
// one byte and looks for the next 0xFF, so a frame that starts inside a corrupt one is still found.
class FrameParser {
 public:
  static const size_t CAPACITY = 2048;

  // Appends bytes; if the buffer would overflow, the oldest bytes are dropped.
  void push(const uint8_t *data, size_t len);
  // The next complete frame's payload, or false if no complete frame is buffered yet.
  // The payload stays valid until the next push() or next_frame().
  bool next_frame(const uint8_t *&payload, size_t &len);

  void set_bypass_crc(bool bypass) { this->bypass_crc_ = bypass; }
  uint32_t crc_errors() const { return this->crc_errors_; }
  uint32_t dropped_bytes() const { return this->dropped_bytes_; }
  size_t buffered() const { return this->size_ - this->start_; }

 protected:
  void compact_();
  uint8_t buf_[CAPACITY];
  size_t start_{0};
  size_t size_{0};
  size_t consume_{0};  // bytes of the last returned frame, dropped on the next call
  bool bypass_crc_{false};
  uint32_t crc_errors_{0};
  uint32_t dropped_bytes_{0};
};

// Register value types; the low nibble is the size in bytes
enum ValueType : uint8_t {
  TYPE_UINT8 = 0x11,
  TYPE_INT8 = 0x21,
  TYPE_UINT16 = 0x12,
  TYPE_INT16 = 0x22,
  TYPE_UINT32 = 0x14,
  TYPE_INT32 = 0x24,
  TYPE_UINT64 = 0x18,
  TYPE_INT64 = 0x28,
  TYPE_FLOAT = 0x34,
  TYPE_DOUBLE = 0x38,
};
inline size_t type_size(uint8_t type) { return type & 0x0F; }

// Little-endian readers
inline uint16_t get_u16(const uint8_t *p) { return (uint16_t) (p[0] | (p[1] << 8)); }
inline int16_t get_i16(const uint8_t *p) { return (int16_t) get_u16(p); }
inline uint32_t get_u32(const uint8_t *p) {
  return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}
inline int32_t get_i32(const uint8_t *p) { return (int32_t) get_u32(p); }
inline uint64_t get_u64(const uint8_t *p) { return (uint64_t) get_u32(p) | ((uint64_t) get_u32(p + 4) << 32); }
inline int64_t get_i64(const uint8_t *p) { return (int64_t) get_u64(p); }
float get_f32(const uint8_t *p);

// A register value as a number, by its wire type. False for unknown types.
bool decode_value(uint8_t type, const uint8_t *p, double &out);

// One register in a READ_REGISTERS_RESPONSE: id, type and a pointer to its value bytes
struct RegisterValue {
  uint16_t id;
  uint8_t type;
  const uint8_t *value;
};
// Walks the registers of a READ_REGISTERS_RESPONSE payload (starting at its type byte).
// Stops at the first register that doesn't fit; returns how many were read.
size_t parse_registers(const uint8_t *payload, size_t len, std::vector<RegisterValue> &out);

// Cell voltages and balancing times (encode_int16_delta_array on the BMS): per value either
// two bytes with bit 7 of the first set, a 15-bit big-endian absolute value, or one byte with
// bit 7 clear, a 7-bit signed delta from the previous value (the first starts from 0).
// signed_absolute: absolute values are 15-bit signed (cell mV, -1 = no reading) rather than
// unsigned (balancing seconds, 0..32767). Returns how many values were decoded.
size_t decode_delta_array(const uint8_t *data, size_t len, size_t count, bool signed_absolute,
                          std::vector<int32_t> &out);

// One event in a READ_EVENTS_RESPONSE (22 bytes)
struct EventRecord {
  uint16_t type;
  uint16_t level;
  uint16_t count;
  uint64_t timestamp;
  uint64_t data;
};
static const size_t EVENT_RECORD_SIZE = 22;
// The BMS stops a reply before tx_idx + 22 > 250, so a full reply holds this many events
static const uint16_t EVENTS_PER_FULL_REPLY = 11;
// A READ_EVENTS_RESPONSE payload: header fields and the records that fit in it
bool parse_events(const uint8_t *payload, size_t len, uint16_t &next_index, std::vector<EventRecord> &out);

}  // namespace protocol
}  // namespace hmi_bms
}  // namespace esphome
