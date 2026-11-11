// Host tests for components/hmi_bms/hmi_protocol.*: run tests/run_tests.sh (g++, e.g. in WSL).
// The encoders here copy the BMS side (bmstest1/bms/protocols/hmi_serial/hmi_serial.c and
// drivers/comms/duart.c) so the decoder is checked against what the BMS really sends.

#include "../components/hmi_bms/hmi_protocol.h"
#include "../components/hmi_bms/hmi_map.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace esphome::hmi_bms;
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

// hmi_build_read_array_response, for an array of int16 or float values (hmi_arrays.c): as many
// elements from start as fit in 256 bytes; none past the end, or before the first reading of a
// timestamped array (sample time 0)
template<typename T>
static std::vector<uint8_t> bms_array_reply(uint8_t array_id, uint8_t encoding, const std::vector<T> &values,
                                            uint16_t start, uint32_t sample_time, bool timestamped = true) {
  std::vector<uint8_t> tx = {0x89, 0x01, array_id, encoding};
  put_u16(tx, start);
  put_u16(tx, (uint16_t) values.size());
  tx.push_back(0);  // count
  for (int i = 0; i < 4; i++) tx.push_back((sample_time >> (8 * i)) & 0xFF);
  uint8_t count = 0;
  if (start < values.size() && !(timestamped && sample_time == 0)) {
    size_t remaining = values.size() - start;
    if (remaining > 255) remaining = 255;
    for (size_t i = 0; i < remaining && tx.size() + sizeof(T) <= 256; i++) {
      uint8_t raw[sizeof(T)];
      memcpy(raw, &values[start + i], sizeof(T));
      tx.insert(tx.end(), raw, raw + sizeof(T));
      count++;
    }
  }
  tx[8] = count;
  return tx;
}

static void test_array_cells() {
  // 150 cell slots as int16 mV: more than one page (121 fit in a frame), -1 = no reading
  std::vector<int16_t> cells(150, -1);
  for (int i = 0; i < 96; i++) cells[i] = (int16_t) (3600 + i);
  cells[20] = 0;
  std::vector<double> got;
  uint16_t start = 0;
  int pages = 0;
  while (pages < 5) {
    auto reply = bms_array_reply(1, TYPE_INT16, cells, start, 123456);
    ArrayPage page;
    CHECK(parse_array_page(reply.data(), reply.size(), page));
    CHECK(page.array_id == 1 && page.encoding == TYPE_INT16 && page.start == start && page.total == 150);
    CHECK(page.sample_time == 123456);
    for (size_t i = 0; i < page.count; i++) {
      double v;
      CHECK(array_element(page, i, v));
      got.push_back(v);
    }
    double v;
    CHECK(!array_element(page, page.count, v));
    pages++;
    start = (uint16_t) (start + page.count);
    if (start >= page.total || page.count == 0) break;
  }
  CHECK(pages == 2);
  bool same = got.size() == cells.size();
  for (size_t i = 0; same && i < cells.size(); i++) same = got[i] == cells[i];
  CHECK(same);

  auto req = read_array_request(0x01, 1, 300);
  CHECK(req == (std::vector<uint8_t>{0x09, 0x01, 1, 0x2C, 0x01}));
}

static void test_array_floats_and_edges() {
  std::vector<float> temps = {21.5f, -3.25f, 40.0f};
  auto reply = bms_array_reply(3, TYPE_FLOAT, temps, 0, 5000);
  ArrayPage page;
  double v;
  CHECK(parse_array_page(reply.data(), reply.size(), page) && page.count == 3 && page.total == 3);
  CHECK(array_element(page, 1, v) && v == -3.25);

  // No reading yet: the header alone, no elements
  reply = bms_array_reply(3, TYPE_FLOAT, temps, 0, 0);
  CHECK(parse_array_page(reply.data(), reply.size(), page) && page.count == 0 && page.total == 3);
  // Past the end
  reply = bms_array_reply(3, TYPE_FLOAT, temps, 3, 5000);
  CHECK(parse_array_page(reply.data(), reply.size(), page) && page.count == 0);
  // An array the BMS doesn't have: encoding 0, no elements
  std::vector<uint8_t> unknown = {0x89, 0x01, 99, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  CHECK(parse_array_page(unknown.data(), unknown.size(), page) && page.total == 0 && page.count == 0);

  // Refused: a short header, elements cut off, a packed or unknown encoding
  reply = bms_array_reply(3, TYPE_FLOAT, temps, 0, 5000);
  CHECK(!parse_array_page(reply.data(), 12, page));
  CHECK(!parse_array_page(reply.data(), reply.size() - 1, page));
  reply[3] = TYPE_FLOAT | 0x80;
  CHECK(!parse_array_page(reply.data(), reply.size(), page));
  reply[3] = 0x44;
  CHECK(!parse_array_page(reply.data(), reply.size(), page));
}

// hmi_build_list_registers_response (hmi_registers.c): entries from start that fit in 256 bytes
static std::vector<uint8_t> bms_register_list_reply(const std::vector<RegisterInfo> &regs, uint16_t start) {
  std::vector<uint8_t> tx = {0x8C, 0x01};
  put_u16(tx, start);
  put_u16(tx, (uint16_t) regs.size());
  tx.push_back(0);
  uint8_t count = 0;
  for (size_t i = start; i < regs.size() && tx.size() + 4 <= 256 && count < 255; i++) {
    put_u16(tx, regs[i].id);
    tx.push_back(regs[i].type);
    tx.push_back(regs[i].flags);
    count++;
  }
  tx[6] = count;
  return tx;
}

static void test_register_list() {
  // 80 registers: two pages (62 entries fit in a frame)
  std::vector<RegisterInfo> regs;
  for (uint16_t i = 0; i < 80; i++) {
    regs.push_back({(uint16_t) (0x1000 + i * 3), (uint8_t) (i % 2 ? TYPE_FLOAT : TYPE_UINT16),
                    (uint8_t) (i >= 70 ? (REG_FLAG_WRITABLE | REG_FLAG_SAVED) : 0)});
  }
  std::vector<RegisterInfo> got;
  uint16_t next = 0;
  int pages = 0;
  while (pages < 5) {
    auto reply = bms_register_list_reply(regs, next);
    uint16_t start, total;
    size_t before = got.size();
    CHECK(parse_register_list(reply.data(), reply.size(), start, total, got));
    CHECK(start == next && total == 80);
    pages++;
    next = (uint16_t) got.size();
    if (got.size() >= total || got.size() == before) break;
  }
  CHECK(pages == 2 && got.size() == 80);
  CHECK(got.size() == 80 && got[79].id == regs[79].id && got[79].type == TYPE_FLOAT &&
        got[79].flags == (REG_FLAG_WRITABLE | REG_FLAG_SAVED) && got[0].flags == 0);

  auto reply = bms_register_list_reply(regs, 0);
  uint16_t start, total;
  CHECK(!parse_register_list(reply.data(), 6, start, total, got));
  CHECK(!parse_register_list(reply.data(), reply.size() - 1, start, total, got));
  CHECK(list_registers_request(0x01, 62) == (std::vector<uint8_t>{0x0C, 0x01, 62, 0}));
}

static void test_nack_and_supervisor() {
  std::vector<uint8_t> nack = {0x80, 0x01, 0x05, 1};
  Nack n;
  CHECK(parse_nack(nack.data(), nack.size(), n) && n.request == 0x05 && n.reason == 1);
  CHECK(!parse_nack(nack.data(), 3, n));

  // READ_SUPERVISOR_RESPONSE as hmi_serial.h 2.12 lays it out
  std::vector<uint8_t> sup = {0x8B, 0x01, 3, 2};
  put_u16(sup, 0x0005);
  put_u16(sup, 0x0100);
  put_u16(sup, (uint16_t) (int16_t) -1);
  put_u16(sup, 4150);
  sup.push_back(95);
  sup.push_back(96);
  sup.push_back(2);
  for (uint32_t v : {1234567u, 40u, 0xFFFFFFFFu}) {
    for (int i = 0; i < 4; i++) sup.push_back((v >> (8 * i)) & 0xFF);
  }
  SupervisorStatus st;
  CHECK(sup.size() == 27);
  CHECK(parse_supervisor(sup.data(), sup.size(), st));
  CHECK(st.protocol_version == 3 && st.state == 2 && st.reasons == 5 && st.latch_reasons == 0x0100);
  CHECK(st.cell_min_mv == -1 && st.cell_max_mv == 4150 && st.cells_in_range == 95 && st.cells_in_map == 96);
  CHECK(st.chemistry == 2 && st.scans == 1234567 && st.ms_since_scan == 40 &&
        st.ms_since_bms_status == 0xFFFFFFFF);
  CHECK(!parse_supervisor(sup.data(), 26, st));
}

static void test_events_paging() {
  // 30 event types have happened: three pages (11 + 11 + 8), followed with next_index
  std::vector<uint16_t> counts(HMI_EVENT_COUNT, 0);
  int expected = 0;
  for (int i = 0; i < HMI_EVENT_COUNT; i += 2) {
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
  std::vector<uint16_t> one(HMI_EVENT_COUNT, 0);
  one[47] = 1;
  auto r = bms_events_reply(one, 0);
  uint16_t next;
  std::vector<EventRecord> recs;
  CHECK(parse_events(r.data(), r.size(), next, recs) && recs.size() == 1 && recs[0].type == 47 && next == HMI_EVENT_COUNT);
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
  test_array_cells();
  test_array_floats_and_edges();
  test_register_list();
  test_nack_and_supervisor();
  test_events_paging();
  printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
