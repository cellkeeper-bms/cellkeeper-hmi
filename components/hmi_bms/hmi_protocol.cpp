#include "hmi_protocol.h"
#include "hmi_map.h"

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

bool parse_array_page(const uint8_t *payload, size_t len, ArrayPage &out) {
  // [0] message type, [1] address, [2] array id, [3] encoding, [4..5] start, [6..7] total,
  // [8] elements in this page, [9..12] sample time, then the elements
  if (len < ARRAY_PAGE_HEADER_SIZE) return false;
  out.array_id = payload[2];
  out.encoding = payload[3];
  out.start = get_u16(payload + 4);
  out.total = get_u16(payload + 6);
  out.count = payload[8];
  out.sample_time = get_u32(payload + 9);
  out.elements = payload + ARRAY_PAGE_HEADER_SIZE;
  if (out.count == 0) return true;
  const size_t size = type_size(out.encoding);
  if ((out.encoding & 0x80) || size == 0) return false;
  if (ARRAY_PAGE_HEADER_SIZE + (size_t) out.count * size > len) return false;
  double unused;
  return decode_value(out.encoding, out.elements, unused);
}

bool array_element(const ArrayPage &page, size_t index, double &out) {
  if (index >= page.count) return false;
  return decode_value(page.encoding, page.elements + index * type_size(page.encoding), out);
}

std::vector<uint8_t> read_array_request(uint8_t address, uint8_t array_id, uint16_t start) {
  return {HMI_MSG_READ_ARRAY, address, array_id, (uint8_t) (start & 0xFF), (uint8_t) (start >> 8)};
}

bool parse_register_list(const uint8_t *payload, size_t len, uint16_t &start, uint16_t &total,
                         std::vector<RegisterInfo> &out) {
  // [0] message type, [1] address, [2..3] start, [4..5] total, [6] entries, then id (2), type, flags
  static const size_t HEADER = 7, ENTRY = 4;
  if (len < HEADER) return false;
  start = get_u16(payload + 2);
  total = get_u16(payload + 4);
  const size_t count = payload[6];
  if (HEADER + count * ENTRY > len) return false;
  for (size_t i = 0; i < count; i++) {
    const uint8_t *e = payload + HEADER + i * ENTRY;
    out.push_back({get_u16(e), e[2], e[3]});
  }
  return true;
}

std::vector<uint8_t> list_registers_request(uint8_t address, uint16_t start) {
  return {HMI_MSG_LIST_REGISTERS, address, (uint8_t) (start & 0xFF), (uint8_t) (start >> 8)};
}

bool parse_nack(const uint8_t *payload, size_t len, Nack &out) {
  if (len < 4) return false;
  out.request = payload[2];
  out.reason = payload[3];
  return true;
}

bool parse_supervisor(const uint8_t *payload, size_t len, SupervisorStatus &out) {
  if (len < 27) return false;
  const uint8_t *p = payload + 2;
  out.protocol_version = p[0];
  out.state = p[1];
  out.reasons = get_u16(p + 2);
  out.latch_reasons = get_u16(p + 4);
  out.cell_min_mv = get_i16(p + 6);
  out.cell_max_mv = get_i16(p + 8);
  out.cells_in_range = p[10];
  out.cells_in_map = p[11];
  out.chemistry = p[12];
  out.scans = get_u32(p + 13);
  out.ms_since_scan = get_u32(p + 17);
  out.ms_since_bms_status = get_u32(p + 21);
  return true;
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
