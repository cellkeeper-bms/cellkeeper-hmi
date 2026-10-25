// Host tests for components/hmi_bms/hmi_protocol.*: run tests/run_tests.sh (g++, e.g. in WSL).
// The encoders here copy the BMS side (bmstest1/bms/protocols/hmi_serial/hmi_serial.c and
// drivers/comms/duart.c) so the decoder is checked against what the BMS really sends.

#include "../components/hmi_bms/hmi_protocol.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace esphome::hmi_bms::protocol;

static int failures = 0;
static int checks = 0;
#define CHECK(cond)                                                     \
  do {                                                                  \
    checks++;                                                           \
    if (!(cond)) {                                                      \
      failures++;                                                       \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
    }                                                                   \
  } while (0)

// --- BMS-side encoders ---

// duart_send_packet: 0xFF, len-1, payload, CRC low, CRC high
static std::vector<uint8_t> bms_frame(const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> buf = {0xFF, (uint8_t) (payload.size() - 1)};
  buf.insert(buf.end(), payload.begin(), payload.end());
  uint16_t crc = crc16_modbus(buf.data(), buf.size());
  buf.push_back(crc & 0xFF);
  buf.push_back(crc >> 8);
  return buf;
}

// encode_int16_delta_array
static size_t bms_encode_int16_delta_array(uint8_t *buf, const int16_t *values, size_t count) {
  int16_t last_value = 0;
  size_t idx = 0;
  for (size_t i = 0; i < count; i++) {
    int16_t value = values[i];
    int16_t delta = value - last_value;
    if (delta >= -64 && delta <= 63) {
      buf[idx++] = (delta & 0x7F);
    } else {
      buf[idx++] = ((value >> 8) & 0xFF) | 0x80;
      buf[idx++] = (value >> 0) & 0xFF;
    }
    last_value = value;
  }
  return idx;
}

static void put_u16(std::vector<uint8_t> &b, uint16_t v) {
  b.push_back(v & 0xFF);
  b.push_back(v >> 8);
}
static void put_u64(std::vector<uint8_t> &b, uint64_t v) {
  for (int i = 0; i < 8; i++) b.push_back((v >> (8 * i)) & 0xFF);
}

// hmi_handle_read_events for slots with count > 0 (types[i] has counts[i]), from start_index
static std::vector<uint8_t> bms_events_reply(const std::vector<uint16_t> &counts, uint16_t start_index) {
  std::vector<uint8_t> tx = {0x86, 0x01, 0, 0, 0, 0};
  uint16_t count = 0;
  uint16_t i;
  for (i = start_index; i < counts.size(); i++) {
    if (counts[i] > 0) {
      if (tx.size() + 22 > 250) break;
      put_u16(tx, i);
      put_u16(tx, i % 5);  // level
      put_u16(tx, counts[i]);
      put_u64(tx, 1000 + i);
      put_u64(tx, 0xABCD0000 + i);
      count++;
    }
  }
  tx[2] = i & 0xFF;
  tx[3] = i >> 8;
  tx[4] = count & 0xFF;
  tx[5] = count >> 8;
  return tx;
}

// --- tests ---

static void test_crc() {
  // Modbus CRC16 check value for "123456789" is 0x4B37
  const char *s = "123456789";
  CHECK(crc16_modbus((const uint8_t *) s, 9) == 0x4B37);
  CHECK(crc16_modbus(nullptr, 0) == 0xFFFF);
}

static void test_build_frame() {
  std::vector<uint8_t> p = {0x04, 0x01, 0x03, 0x00};
  CHECK(build_frame(p) == bms_frame(p));
  CHECK(build_frame(p)[0] == 0xFF);
  CHECK(build_frame(p)[1] == 3);
  CHECK(build_frame(nullptr, 0).empty());
  std::vector<uint8_t> big(257, 0x11);
  CHECK(build_frame(big).empty());
  std::vector<uint8_t> max(256, 0x22);
  CHECK(build_frame(max).size() == 260);
}

static std::vector<std::vector<uint8_t>> drain(FrameParser &fp) {
  std::vector<std::vector<uint8_t>> out;
  const uint8_t *p;
  size_t n;
  while (fp.next_frame(p, n)) out.emplace_back(p, p + n);
  return out;
}

static void test_parser_basic() {
  FrameParser fp;
  std::vector<uint8_t> a = {0x84, 0x01, 0x03, 0x00, 0x12, 0x10, 0x27};
  std::vector<uint8_t> b = {0x81, 0x01, 0x00, 1, 2, 3, 4, 5, 6, 7, 8};
  std::vector<uint8_t> stream = bms_frame(a);
  auto fb = bms_frame(b);
  stream.insert(stream.end(), fb.begin(), fb.end());
  fp.push(stream.data(), stream.size());
  auto frames = drain(fp);
  CHECK(frames.size() == 2);
  CHECK(frames.size() == 2 && frames[0] == a && frames[1] == b);
  CHECK(fp.crc_errors() == 0);
  CHECK(fp.buffered() == 0);
}

static void test_parser_split_bytes() {
  // Frames arriving one byte at a time
  FrameParser fp;
  std::vector<uint8_t> a(200);
  for (size_t i = 0; i < a.size(); i++) a[i] = (uint8_t) (i * 7);
  a[0] = 0x88;
  auto f = bms_frame(a);
  size_t got = 0;
  for (size_t i = 0; i < f.size(); i++) {
    fp.push(&f[i], 1);
    const uint8_t *p;
    size_t n;
    if (fp.next_frame(p, n)) {
      got++;
      CHECK(i == f.size() - 1);
      CHECK(n == a.size() && memcmp(p, a.data(), n) == 0);
    }
  }
  CHECK(got == 1);
}

static void test_parser_junk_and_bad_crc() {
  FrameParser fp;
  std::vector<uint8_t> a = {0x84, 0x01, 0x10, 0x00, 0x11, 0x05};
  std::vector<uint8_t> stream = {0x00, 0x13, 0x37};  // junk before
  auto bad = bms_frame(a);
  bad[4] ^= 0x01;  // corrupt one payload byte
  stream.insert(stream.end(), bad.begin(), bad.end());
  auto good = bms_frame(a);
  stream.insert(stream.end(), good.begin(), good.end());
  fp.push(stream.data(), stream.size());
  auto frames = drain(fp);
  CHECK(frames.size() == 1 && frames[0] == a);
  CHECK(fp.crc_errors() >= 1);
}

static void test_parser_byte_swapped_crc_rejected() {
  FrameParser fp;
  std::vector<uint8_t> a = {0x84, 0x01, 0x10, 0x00, 0x11, 0x05};
  auto f = bms_frame(a);
  std::swap(f[f.size() - 1], f[f.size() - 2]);
  fp.push(f.data(), f.size());
  CHECK(drain(fp).empty());
}

static void test_parser_frame_inside_corrupt_frame() {
  // A frame header whose length points past a real frame that follows: the corrupt one fails
  // its CRC once enough bytes arrive, then the real frame is found from the next sync byte.
  FrameParser fp;
  std::vector<uint8_t> a = {0x85, 0x01, 0x02, 0x81, 0x00, 0x05};
  std::vector<uint8_t> stream = {0xFF, 20};
  auto good = bms_frame(a);
  stream.insert(stream.end(), good.begin(), good.end());
  std::vector<uint8_t> filler(30, 0x00);
  stream.insert(stream.end(), filler.begin(), filler.end());
  fp.push(stream.data(), stream.size());
  auto frames = drain(fp);
  CHECK(frames.size() == 1 && frames[0] == a);
}

static void test_parser_payload_survives_until_next_call() {
  FrameParser fp;
  std::vector<uint8_t> a = {0x84, 0x01, 0x02, 0x00, 0x14, 1, 2, 3, 4};
  auto f = bms_frame(a);
  fp.push(f.data(), f.size());
  const uint8_t *p;
  size_t n;
  CHECK(fp.next_frame(p, n));
  CHECK(n == a.size() && memcmp(p, a.data(), n) == 0);
  CHECK(!fp.next_frame(p, n));
}

static void test_parser_overflow() {
  FrameParser fp;
  std::vector<uint8_t> junk(FrameParser::CAPACITY + 500, 0x55);
  fp.push(junk.data(), junk.size());
  CHECK(fp.buffered() <= FrameParser::CAPACITY);
  std::vector<uint8_t> a = {0x84, 0x01};
  auto f = bms_frame(a);
  fp.push(f.data(), f.size());
  auto frames = drain(fp);
  CHECK(frames.size() == 1 && frames[0] == a);
}

static void test_values() {
  uint8_t b[8];
  b[0] = 0xFE;
  double v;
  CHECK(decode_value(TYPE_UINT8, b, v) && v == 254);
  CHECK(decode_value(TYPE_INT8, b, v) && v == -2);
  b[0] = 0x18; b[1] = 0xFC;  // 0xFC18 = 64536 / -1000
  CHECK(decode_value(TYPE_UINT16, b, v) && v == 64536);
  CHECK(decode_value(TYPE_INT16, b, v) && v == -1000);
  b[0] = 0x60; b[1] = 0x79; b[2] = 0xFE; b[3] = 0xFF;  // -100000
  CHECK(decode_value(TYPE_INT32, b, v) && v == -100000);
  CHECK(decode_value(TYPE_UINT32, b, v) && v == 4294867296.0);
  float f = 23.125f;
  memcpy(b, &f, 4);
  CHECK(decode_value(TYPE_FLOAT, b, v) && v == 23.125);
  float nan = NAN;
  memcpy(b, &nan, 4);
  CHECK(decode_value(TYPE_FLOAT, b, v) && std::isnan(v));
  int64_t big = -123456789012LL;
  memcpy(b, &big, 8);
  CHECK(decode_value(TYPE_INT64, b, v) && v == -123456789012.0);
  CHECK(!decode_value(0x99, b, v));
  CHECK(type_size(TYPE_UINT64) == 8 && type_size(TYPE_UINT8) == 1);
}

static void test_registers() {
  // 0x84, addr, then SoC (3, uint16 4532), current (4, int32 -12345), temp min (10, float 21.5)
  std::vector<uint8_t> p = {0x84, 0x01};
  put_u16(p, 3); p.push_back(TYPE_UINT16); put_u16(p, 4532);
  put_u16(p, 4); p.push_back(TYPE_INT32);
  int32_t cur = -12345;
  for (int i = 0; i < 4; i++) p.push_back((cur >> (8 * i)) & 0xFF);
  put_u16(p, 10); p.push_back(TYPE_FLOAT);
  float t = 21.5f;
  uint8_t tb[4];
  memcpy(tb, &t, 4);
  p.insert(p.end(), tb, tb + 4);
  std::vector<RegisterValue> regs;
  CHECK(parse_registers(p.data(), p.size(), regs) == 3);
  double v;
  CHECK(regs.size() == 3 && regs[0].id == 3 && decode_value(regs[0].type, regs[0].value, v) && v == 4532);
  CHECK(regs.size() == 3 && regs[1].id == 4 && decode_value(regs[1].type, regs[1].value, v) && v == -12345);
  CHECK(regs.size() == 3 && regs[2].id == 10 && decode_value(regs[2].type, regs[2].value, v) && v == 21.5);
  // A register cut off by the end of the reply is not returned
  CHECK(parse_registers(p.data(), p.size() - 1, regs) == 2);
  // Size 0 type stops parsing
  std::vector<uint8_t> q = {0x84, 0x01, 3, 0, 0x10, 0, 0};
  CHECK(parse_registers(q.data(), q.size(), regs) == 0);
}

static void test_delta_cells() {
  // 120 cells like the BMS sends: 15 real ones, then -1 ("no reading"), plus a big jump
  int16_t cells[120];
  for (int i = 0; i < 120; i++) cells[i] = -1;
  int16_t real[15] = {3632, 3619, 3640, 3682, 3682, 3626, 3632, 3643, 3628, 3627, 3626, 3682, 3639, 3630, 3621};
  memcpy(cells, real, sizeof(real));
  cells[20] = 0;
  cells[21] = 4200;
  uint8_t buf[260];
  size_t n = bms_encode_int16_delta_array(buf, cells, 120);
  std::vector<int32_t> out;
  CHECK(decode_delta_array(buf, n, 120, true, out) == 120);
  bool same = out.size() == 120;
  for (int i = 0; same && i < 120; i++) same = out[i] == cells[i];
  CHECK(same);
  // Truncated input decodes what fits
  CHECK(decode_delta_array(buf, 3, 120, true, out) >= 1);
}

static void test_delta_balancing() {
  // Balancing seconds up to 32767 decode as unsigned (bit 14 set would be negative if signed)
  int16_t t[120] = {0};
  t[0] = 20000;
  t[1] = 19990;
  t[5] = 32767;
  t[6] = 1;
  uint8_t buf[260];
  size_t n = bms_encode_int16_delta_array(buf, t, 120);
  std::vector<int32_t> out;
  CHECK(decode_delta_array(buf, n, 120, false, out) == 120);
  CHECK(out.size() == 120 && out[0] == 20000 && out[1] == 19990 && out[5] == 32767 && out[6] == 1 && out[7] == 0);
  CHECK(decode_delta_array(buf, n, 120, true, out) == 120 && out[0] != 20000);  // why it must be unsigned
}

static void test_events_paging() {
  // 30 event types have happened: three pages (11 + 11 + 8), followed with next_index
  std::vector<uint16_t> counts(61, 0);
  int expected = 0;
  for (int i = 0; i < 61; i += 2) {
    counts[i] = (uint16_t) (i + 1);
    expected++;
  }
  std::vector<int> seen;
  uint16_t start = 0;
  int pages = 0;
  while (true) {
    auto reply = bms_events_reply(counts, start);
    uint16_t next;
    std::vector<EventRecord> recs;
    CHECK(parse_events(reply.data(), reply.size(), next, recs));
    for (auto &r : recs) {
      seen.push_back(r.type);
      CHECK(r.count == r.type + 1);
      CHECK(r.timestamp == 1000u + r.type);
      CHECK(r.data == 0xABCD0000u + r.type);
    }
    pages++;
    CHECK(recs.size() <= EVENTS_PER_FULL_REPLY);
    if (recs.size() < EVENTS_PER_FULL_REPLY || pages > 10) break;
    start = next;
  }
  CHECK((int) seen.size() == expected);
  CHECK(pages == 3);
  // One page when few events
  std::vector<uint16_t> one(61, 0);
  one[47] = 1;
  auto r = bms_events_reply(one, 0);
  uint16_t next;
  std::vector<EventRecord> recs;
  CHECK(parse_events(r.data(), r.size(), next, recs) && recs.size() == 1 && recs[0].type == 47 && next == 61);
  CHECK(!parse_events(r.data(), 5, next, recs));
}

int main() {
  test_crc();
  test_build_frame();
  test_parser_basic();
  test_parser_split_bytes();
  test_parser_junk_and_bad_crc();
  test_parser_byte_swapped_crc_rejected();
  test_parser_frame_inside_corrupt_frame();
  test_parser_payload_survives_until_next_call();
  test_parser_overflow();
  test_values();
  test_registers();
  test_delta_cells();
  test_delta_balancing();
  test_events_paging();
  printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
