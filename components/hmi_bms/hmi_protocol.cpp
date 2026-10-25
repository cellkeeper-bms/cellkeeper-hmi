#include "hmi_protocol.h"

#include <cstring>

namespace esphome {
namespace hmi_bms {
namespace protocol {

uint16_t crc16_modbus(const uint8_t *data, size_t len) {
  // Modbus CRC16 (poly 0x8005, reflected 0xA001), init 0xFFFF
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t) data[i];
    for (uint8_t j = 0; j < 8; j++) {
      crc = (crc & 0x0001) ? (uint16_t) ((crc >> 1) ^ 0xA001) : (uint16_t) (crc >> 1);
    }
  }
  return crc;
}

std::vector<uint8_t> build_frame(const uint8_t *payload, size_t len) {
  std::vector<uint8_t> frame;
  if (len == 0 || len > MAX_PAYLOAD) return frame;
  frame.reserve(len + FRAME_OVERHEAD);
  frame.push_back(SYNC);
  frame.push_back((uint8_t) (len - 1));
  frame.insert(frame.end(), payload, payload + len);
  uint16_t crc = crc16_modbus(frame.data(), frame.size());
  frame.push_back(crc & 0xFF);
  frame.push_back(crc >> 8);
  return frame;
}

void FrameParser::compact_() {
  if (this->start_ == 0) return;
  memmove(this->buf_, this->buf_ + this->start_, this->size_ - this->start_);
  this->size_ -= this->start_;
  this->start_ = 0;
}

void FrameParser::push(const uint8_t *data, size_t len) {
  this->start_ += this->consume_;
  this->consume_ = 0;
  if (len >= CAPACITY) {
    // Only the newest CAPACITY bytes can matter
    this->dropped_bytes_ += (this->size_ - this->start_) + (len - CAPACITY);
    data += len - CAPACITY;
    len = CAPACITY;
    this->start_ = this->size_ = 0;
  }
  this->compact_();
  if (this->size_ + len > CAPACITY) {
    size_t drop = this->size_ + len - CAPACITY;
    this->dropped_bytes_ += drop;
    this->start_ = drop;
    this->compact_();
  }
  memcpy(this->buf_ + this->size_, data, len);
  this->size_ += len;
}

bool FrameParser::next_frame(const uint8_t *&payload, size_t &len) {
  this->start_ += this->consume_;
  this->consume_ = 0;
  while (true) {
    // Skip to the next sync byte
    while (this->start_ < this->size_ && this->buf_[this->start_] != SYNC) {
      this->start_++;
      this->dropped_bytes_++;
    }
    size_t avail = this->size_ - this->start_;
    if (avail < 2) return false;
    const uint8_t *f = this->buf_ + this->start_;
    size_t payload_len = (size_t) f[1] + 1;
    size_t frame_len = payload_len + FRAME_OVERHEAD;
    if (avail < frame_len) return false;
    uint16_t received = (uint16_t) (f[payload_len + 2] | (f[payload_len + 3] << 8));
    uint16_t computed = crc16_modbus(f, payload_len + 2);
    if (received == computed || this->bypass_crc_) {
      payload = f + 2;
      len = payload_len;
      this->consume_ = frame_len;
      return true;
    }
    // Corrupt: drop this sync byte and look again from the next byte
    this->crc_errors_++;
    this->start_++;
    this->dropped_bytes_++;
  }
}

float get_f32(const uint8_t *p) {
  uint32_t raw = get_u32(p);
  float f;
  memcpy(&f, &raw, sizeof(f));
  return f;
}

bool decode_value(uint8_t type, const uint8_t *p, double &out) {
  switch (type) {
    case TYPE_UINT8: out = p[0]; return true;
    case TYPE_INT8: out = (int8_t) p[0]; return true;
    case TYPE_UINT16: out = get_u16(p); return true;
    case TYPE_INT16: out = get_i16(p); return true;
    case TYPE_UINT32: out = get_u32(p); return true;
    case TYPE_INT32: out = get_i32(p); return true;
    case TYPE_UINT64: out = (double) get_u64(p); return true;
    case TYPE_INT64: out = (double) get_i64(p); return true;
    case TYPE_FLOAT: out = get_f32(p); return true;
    case TYPE_DOUBLE: {
      uint64_t raw = get_u64(p);
      double d;
      memcpy(&d, &raw, sizeof(d));
      out = d;
      return true;
    }
    default: return false;
  }
}

size_t parse_registers(const uint8_t *payload, size_t len, std::vector<RegisterValue> &out) {
  out.clear();
  // payload[0] = message type, payload[1] = device address, then id (2), type (1), value (n)...
  size_t offset = 2;
  while (offset + 3 <= len) {
    uint16_t id = get_u16(payload + offset);
    uint8_t type = payload[offset + 2];
    size_t size = type_size(type);
    offset += 3;
    if (size == 0 || offset + size > len) break;
    out.push_back({id, type, payload + offset});
    offset += size;
  }
  return out.size();
}

size_t decode_delta_array(const uint8_t *data, size_t len, size_t count, bool signed_absolute,
                          std::vector<int32_t> &out) {
  out.clear();
  size_t offset = 0;
  int32_t last = 0;
  for (size_t i = 0; i < count && offset < len; i++) {
    int32_t value;
    if (data[offset] & 0x80) {
      if (offset + 1 >= len) break;
      uint16_t v = (uint16_t) (((data[offset] & 0x7F) << 8) | data[offset + 1]);
      if (signed_absolute && (v & 0x4000)) {
        value = (int16_t) (v | 0x8000);
      } else {
        value = v;
      }
      offset += 2;
    } else {
      uint8_t v = data[offset] & 0x7F;
      int8_t delta = (v & 0x40) ? (int8_t) (v | 0x80) : (int8_t) v;
      value = last + delta;
      offset += 1;
    }
    out.push_back(value);
    last = value;
  }
  return out.size();
}

bool parse_events(const uint8_t *payload, size_t len, uint16_t &next_index, std::vector<EventRecord> &out) {
  out.clear();
  // payload[0] = message type, [1] = address, [2..3] = next index, [4..5] = events in this reply
  if (len < 6) return false;
  next_index = get_u16(payload + 2);
  uint16_t count = get_u16(payload + 4);
  size_t offset = 6;
  for (uint16_t i = 0; i < count && offset + EVENT_RECORD_SIZE <= len; i++, offset += EVENT_RECORD_SIZE) {
    const uint8_t *r = payload + offset;
    out.push_back({get_u16(r), get_u16(r + 2), get_u16(r + 4), get_u64(r + 6), get_u64(r + 14)});
  }
  return true;
}

}  // namespace protocol
}  // namespace hmi_bms
}  // namespace esphome
