// Health: the things that should simply be right. Isolation, the supply rails, temperatures of
// the electronics, the contactors with the supervisor that can cut them, and the links.
#include "ui_draw.h"

#include <algorithm>

namespace hmiui {

static const char *age_text(uint32_t s) {
  if (s < 90) return fmt("%u s ago", (unsigned) s);
  if (s < 5400) return fmt("%u min ago", (unsigned) (s / 60));
  return fmt("%u h %02u min ago", (unsigned) (s / 3600), (unsigned) (s % 3600 / 60));
}

static const char *uptime_text(uint32_t s) {
  if (s >= 86400) return fmt("%u d %02u h", (unsigned) (s / 86400), (unsigned) (s % 86400 / 3600));
  return fmt("%u:%02u:%02u", (unsigned) (s / 3600), (unsigned) (s % 3600 / 60), (unsigned) (s % 60));
}

// A supply rail: its voltage, and a mark for where it sits in the range it should be in
static void rail(Canvas &c, const Theme &t, int x, int y, int w, const char *name, float v, float lo, float hi) {
  const bool known = !std::isnan(v), ok = known && v >= lo && v <= hi;
  row(c, t, x, y, w, name, value(v, "%.2f V"), known && !ok ? t.warning : 0);
  c.round_rect(x, y + 28, w, 4, 2, t.track);
  if (!known) return;
  // The bar spans the allowed range plus a quarter on each side
  const float a = lo - (hi - lo) * 0.25f, b = hi + (hi - lo) * 0.25f;
  c.round_rect(x + w / 6, y + 28, w * 4 / 6, 4, 2, mix(t.card, t.good, 0.45f));
  const int at = x + (int) (clampf((v - a) / (b - a), 0, 1) * w);
  c.round_rect(at - 3, y + 23, 6, 14, 3, ok ? t.good : t.warning);
}

void draw_health(Canvas &c, const Theme &t, const Model &m) {
  top_line(c, t, m, "Health");
  const int gap = 12, cw = (X1 - X0 - 2 * gap) / 3, top_h = 252;
  const int x2 = X0 + cw + gap, x3 = x2 + cw + gap, w3 = X1 - x3;

  // ---- isolation ----
  card(c, t, X0, Y0, cw, top_h);
  c.text(FONT_R14, X0 + 16, Y0 + 12, "ISOLATION", t.caption);
  if (!std::isnan(m.isolation_kohm)) {
    const bool capped = m.isolation_kohm >= 100000.0f, good = m.isolation_kohm >= 500.0f;
    const char *number = m.isolation_kohm >= 1000.0f ? fmt("%.*f", m.isolation_kohm >= 10000.0f ? 0 : 1,
                                                           (double) m.isolation_kohm / 1000.0)
                                                     : fmt("%.0f", (double) m.isolation_kohm);
    // At the top of what the measurement can tell, the value is "at least"
    const int lead = capped ? text_base(c, FONT_R32, X0 + 16, Y0 + 82, "≥", good ? t.good : t.warning) + 8 : 0;
    num_unit(c, X0 + 16 + lead, Y0 + 84, FONT_M48, number, FONT_R24, m.isolation_kohm >= 1000.0f ? "MΩ" : "kΩ",
             good ? t.good : t.warning, t.caption, LEFT, 8);
    c.text(FONT_R16, X0 + 16, Y0 + 94, good ? "Healthy: pack to chassis" : "Low: pack to chassis", good ? t.good : t.warning);
  } else {
    num_unit(c, X0 + 16, Y0 + 84, FONT_M48, "--", FONT_R24, "", t.hint, t.hint);
    c.text(FONT_R16, X0 + 16, Y0 + 94, "Not measured yet", t.caption);
  }
  row(c, t, X0 + 16, Y0 + 134, cw - 32, "Pack + to chassis", value(m.isolation_pos_v, "%.1f V"));
  row(c, t, X0 + 16, Y0 + 166, cw - 32, "Pack − to chassis", value(m.isolation_neg_v, "%.1f V"));
  row(c, t, X0 + 16, Y0 + 198, cw - 32, "Measured", m.isolation_age_s > 0 ? age_text(m.isolation_age_s) : "--");
  c.text(FONT_R14, X0 + 16, Y0 + 226, "Measured while the contactors are open.", t.hint);

  // ---- supplies ----
  card(c, t, x2, Y0, cw, top_h);
  c.text(FONT_R14, x2 + 16, Y0 + 12, "SUPPLIES", t.caption);
  rail(c, t, x2 + 16, Y0 + 42, cw - 32, "3.3 V rail", m.v3v3, 3.15f, 3.45f);
  rail(c, t, x2 + 16, Y0 + 94, cw - 32, "5 V rail", m.v5, 4.75f, 5.40f);
  rail(c, t, x2 + 16, Y0 + 146, cw - 32, "12 V rail", m.v12, 11.0f, 14.5f);
  rail(c, t, x2 + 16, Y0 + 198, cw - 32, "Contactor supply", m.v_contactor, 11.0f, 14.5f);

  // ---- temperatures ----
  card(c, t, x3, Y0, w3, top_h);
  c.text(FONT_R14, x3 + 16, Y0 + 12, "TEMPERATURES", t.caption);
  row(c, t, x3 + 16, Y0 + 44, w3 - 32, "Modules",
      std::isnan(m.temp_min) ? "--" : fmt("%.1f – %.1f °C", (double) m.temp_min, (double) m.temp_max));
  row(c, t, x3 + 16, Y0 + 84, w3 - 32, "BMS chip", value(m.bms_chip_temp, "%.1f °C"));
  row(c, t, x3 + 16, Y0 + 124, w3 - 32, "Current sensor", value(m.shunt_temp, "%.1f °C"));
  {
    // An NTC that reads far below freezing is one that isn't connected
    const bool absent = !std::isnan(m.shunt_ntc_temp) && m.shunt_ntc_temp < -40.0f;
    row(c, t, x3 + 16, Y0 + 164, w3 - 32, "Shunt NTC", absent ? "not connected" : value(m.shunt_ntc_temp, "%.1f °C"),
        absent ? t.caption : 0);
  }
  row(c, t, x3 + 16, Y0 + 204, w3 - 32, "Display chip", value(m.hmi_chip_temp, "%.1f °C"));

  // ---- contactors and the supervisor ----
  const int by = Y0 + top_h + gap, bh = Y1 - by, lw = (X1 - X0 - gap) / 2, rx = X0 + lw + gap, rw = X1 - rx;
  card(c, t, X0, by, lw, bh);
  c.text(FONT_R14, X0 + 16, by + 12, "CONTACTORS AND SUPERVISOR", t.caption);
  const bool fault = m.system_state == SystemState::FAULT;
  row(c, t, X0 + 16, by + 44, lw - 32, "BMS state", system_state_name(m.system_state), fault ? t.fatal : 0);
  row(c, t, X0 + 16, by + 80, lw - 32, "Contactors", m.contactor_state_text[0] ? m.contactor_state_text : "--");
  static const char *const SUPERVISOR[] = {"no answer", "Holding the contactors off", "Allowing the contactors", "Latched off"};
  const int sv = m.supervisor_state < -1 || m.supervisor_state > 2 ? -1 : m.supervisor_state;
  row(c, t, X0 + 16, by + 116, lw - 32, "Supervisor", SUPERVISOR[sv + 1], sv == 1 ? t.good : (sv == -1 ? t.caption : 0));
  if (sv == 0 || sv == 2) {
    c.text(FONT_R16, X0 + 16, by + 152, "Because:", t.caption);
    text_wrap(c, FONT_R16, X0 + 90, by + 152, lw - 106, m.supervisor_reasons, t.text_dim, 3);
  } else if (sv == 1) {
    c.text(FONT_R16, X0 + 16, by + 152, "A second chip that can cut the contactor drivers on its own.", t.hint);
  }

  // ---- links ----
  card(c, t, rx, by, rw, bh);
  c.text(FONT_R14, rx + 16, by + 12, "LINKS", t.caption);
  const bool linked = m.link == Link::OK;
  row(c, t, rx + 16, by + 44, rw - 32, "Display to BMS", linked ? "OK, protocol 2" : "no answer", linked ? t.good : t.warning);
  row(c, t, rx + 16, by + 80, rw - 32, "Checksum errors / timeouts",
      fmt("%u / %u", (unsigned) m.link_crc_errors, (unsigned) m.link_timeouts));
  row(c, t, rx + 16, by + 116, rw - 32, "Inverter", std::isnan(m.inverter_soc) ? "not seen on CAN" : "talking on CAN",
      std::isnan(m.inverter_soc) ? t.caption : t.good);
  row(c, t, rx + 16, by + 152, rw - 32, "Wi-Fi", m.wifi_ssid[0] ? fmt("%s, %d dBm", m.wifi_ssid, m.wifi_rssi) : "not connected");
  row(c, t, rx + 16, by + 188, rw - 32, "Running for: BMS / display",
      fmt("%s / %s", m.bms_uptime_s ? uptime_text(m.bms_uptime_s) : "--", uptime_text(m.hmi_uptime_s)));

  page_end(c, t, m, Page::HEALTH);
}

}  // namespace hmiui
