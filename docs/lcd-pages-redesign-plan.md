# Plan: redesign the LCD's secondary pages for a 7" screen

**Status:** phases 1-8 built on branch `lcd-redesign` (2026-10-25), being tested on the desk
Waveshare 7B. Decisions: Main v2 is the boot page; the classic Main keeps its layout with a slim
tab bar; Events shows all 61 types grouped by severity; Settings and System commands need a
4-digit PIN (`hmi_pin`, from secrets.yaml); after 5 minutes idle the screen goes back to Main v2
and dims to 10 % until touched; 800x480 is left as it is for now.
After the first test the header bar was dropped as too busy: the Events tab shows the count and
colour of active warnings, and a red "NO BMS DATA" tag appears at the top when the link is down.

**Scope:** every LVGL page. **The current main page (`energy_page`) stays as it is.** A new,
re-imagined **Main v2** goes next to it for comparison, and you pick one later.
Target: 7" 1024×600 (Guition JC1060P470, Waveshare ESP32-P4 7B). The 800×480 Waveshare S3 7"
follows at the end.

## Where it is now

`ui_1024x600.yaml` (2,251 lines) has nine pages in one swipe chain:

```
details ← MAIN → commands → registers → balancing → limits → event log → BMS log → wifi
```

| Page | What it holds | Problem |
|---|---|---|
| `energy_page` (**main**) | SoC, power, current, voltage arcs; temps; cells; contactor | **out of scope, unchanged** |
| `details_page` | cell voltage grid, active/balancing, min/avg/max, balance cycle time | good content, isolated from the balancing settings it relates to |
| `debug_commands_page` | Stop, Run, 3 calibrations | dangerous buttons one swipe from main, no confirmation |
| `debug_page` | 22 raw registers ("Reg 6 (Battery V)") | engineering dump; the values exist as named sensors |
| `balancing_page` | 7 balancing settings + active cells, lock | split from `limits_page`; both repeat the lock header |
| `limits_page` | max chg/dis, voltage offsets, working min/max, SoC scaling | same |
| `debug_page_3` | "Detailed event log", usually "No events logged." | almost empty page |
| `log_page` | BMS console log, 50 lines, on/off and rate | useful for service only |
| `wifi_page` | SSID, IP, RSSI, portal reset, reboot | three numbers and two buttons on a whole page |

**Problems overall:**
- **Navigation is a long linear chain.** Reaching Wi-Fi takes 7 swipes. Labels like "<- Event Log | Balancing ->" are the only map, and they're inconsistent.
- **Service pages sit next to main,** with no confirmation on Stop or Run.
- **Settings are spread over two pages,** and the balance-charge settings exist only in HA.
- **Much of the BMS data has no place on screen:**
  - the current-limit breakdown (temperature / cell voltage / working / hard / delta)
  - what's sent to the inverter (SoC, capacity, voltage limits)
  - isolation (Pack± to chassis, Riso, check age)
  - the eight BMB module temperatures, and the shunt and chip temperatures
  - the three SoC estimators, today's energy
  - board revision, firmware builds, supervisor state

## Compared with the HA dashboard (`HA-dashboard.yaml`)

The dashboard has 12 views: Overview, Detail (×3), Cells, Cells 3D, Cells 2D, Configuration,
System, Log, Event, Note. **What to bring to the LCD:**

| HA view | Feature to bring | LCD page |
|---|---|---|
| **Event** | **three tiers of events.** Each BMS event entity reads active with a severity, `INACTIVE (n)` (happened before, n times), or `INACTIVE` (never). HA shows **Active Events** coloured by severity (Fatal `#D50000`, Critical `#F44336`, Warning `#FFC107`, Info `#2196F3`), **Previously Active** in purple (`#9C27B0`) with counts, and **All Event Types** (61) as a grid. | **Events** (new page) |
| **Overview** | active events shown right next to SoC, power and state; quick Run / Stop / Calibrate | **Main v2**: an events banner. The commands stay on System (with confirmation). |
| **Detail** | current limits, what's sent to the inverter, isolation, energy, module temperatures | Power, Health |
| **Cells 2D / 3D** | a heat-map of the cells with balancing highlighted | Cells: the grid coloured as a heat-map |
| **Configuration** | a "Configuration Guide" with the settings grouped (SoC and current, voltage limits, balancing, balance charge, isolation calibration), plus the **settings write status** ("settings applied") | Settings: the same grouping, a status line, and an "i" help pop-up per setting using the guide's text |
| **System** | BMS supplies, board, serial; HMI heap, loop time, uptime, Wi-Fi | Health (supplies) and System (about, network) |
| **Log** | the 50-line BMS log with on/off and rate | System |
| **Note** | Deye, Solax, balancing and event notes | Settings help pop-ups; the event notes go with Events |

The HMI already receives the same event data from the BMS (the event list that feeds the HA
entities), so the LCD can show the three tiers without new BMS work.

## New structure: Main, Main v2 + 6 pages, with a tab bar

```
┌────────────────────────────────────────────── 1024 ──────────────────────────────────────────────┐
│ header 52 px: page title · BMS state chip · contactor chip · lock icon · Wi-Fi icon · time      │
├──────────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                                  │
│ content 476 px: cards in a 12-column grid, 16 px gutters                                         │
│                                                                                                  │
├──────────────────────────────────────────────────────────────────────────────────────────────────┤
│ tab bar 72 px: Main │ Main v2 │ Cells │ Power │ Health │ Events │ Settings │ System (128 px each) │
└──────────────────────────────────────────────────────────────────────────────────────────────────┘
```

- **Tab bar on every page except Main.**
  - On Main, a small "≡" handle (or a swipe up) shows it.
  - Main itself keeps its layout; the bar overlays nothing on it.
  - Swiping left or right still steps between neighbouring tabs.
- **Every page is one tap from any other.** The tab bar also replaces the arrow-text footers.
- **Auto-return to Main** after 5 minutes without a touch, which is configurable.

An events badge on the Events tab (the worst active severity, with a count) is visible on every
page. (Built without the header row: it was too busy on the screen.)

| Page | Built from | Content |
|---|---|---|
| **Main** | `energy_page` | **unchanged** |
| **Main v2** | new, next to Main for now | a re-imagined main page for 7", described below |
| **Cells** | `details_page` plus the status half of `balancing_page` | the full cell grid, coloured by voltage with balancing cells outlined; min / avg / max / Δ; a strip of the 8 module (BMB) temperatures; balancing now (count, cycle time left); balance-charge status ("Floating, next in 7 days") |
| **Power** | new | a **current-limit breakdown**: charge and discharge as horizontal bars per limiter (hard, working, temperature, cell voltage, delta), with the one in force highlighted; **what the inverter sees** (SoC sent, capacity, CVL / DVL); pack internal and output voltage, contactor voltages; SoC: EKF / voltage-based / coulomb; energy today (charged / discharged / net) |
| **Health** | the named half of `debug_page` | **isolation**: Pack+ / Pack− to chassis, Riso, check age, calibrate-zero button; **supplies**: 3V3 / 5V / 12V / contactor against their limits; **temperatures**: BMS chip, shunt die, shunt NTC, HMI; supervisor state |
| **Events** | `debug_page_3`, HA's Event view | **Active**: one tile per active event, coloured by severity as in HA, with level and count, worst first. **Previously active**: purple tiles with their counts. **All event types**: a compact grid of all 61, by severity group, scrollable. Tap an event for a short explanation (from the dashboard's event notes). |
| **Settings** | `limits_page`, `balancing_page`, balance charge (HA-only today) | one lock for the page; four cards: **Charge limits** (max chg/dis current, working min/max, soft min/max, voltage offsets), **SoC** (scaling min/max, set SoC), **Balancing** (start voltage, minimum balancing voltage, min offset, period, periods/mV), **Balance charge** (interval, start hour, duration, float and balance offsets, start / stop now) |
| **System** | `debug_commands_page`, `log_page`, `wifi_page`, `debug_page` | **commands**: Run / Stop / calibrations, each with a confirm step; **BMS log** viewer (on/off, rate, scrolling); **network**: SSID, IP, RSSI, BSSID, portal reset, reboot HMI; **about**: board revision, serial, BMS and HMI firmware builds, uptimes, heap, loop time; a **"Raw registers"** toggle that shows the old register dump for debugging |

**Removed as separate pages:** registers (`debug_page`), event log (`debug_page_3`, replaced by
Events), Wi-Fi (`wifi_page`), commands (`debug_commands_page`). Their content moves as above.
**Nine pages become seven,** plus Main v2 while you compare the two mains.

### Main v2: the main page re-imagined for 7"

Built from scratch with the new styles. It reads at a glance from across the room, and answers
"is the battery OK, what is it doing, what's limiting it":

```
┌──────────────────────────────────────── 1024 × 528 (above the tab bar) ─────────────────────────────────────┐
│ ▲ events banner (only when something is active): "WARNING · Cell voltage high (2)"   → tap opens Events       │
├───────────────────────────┬─────────────────────────────────────────────────────────────────────────────────┤
│        ╭───────╮          │  ⇩ CHARGING   6.8 kW      378.3 V   +18.0 A                                       │
│       │  98 %  │          │  ──────────────────────────────────────────  (charge/discharge bar, ±max kW)      │
│        ╰───────╯          │  Limited by: temperature · 43 A of 50 A                                           │
│   SoC ring (Δ to target)  │                                                                                   │
│   OPERATING · CLOSED      │  Cells 3.939–3.945 V  Δ 6 mV  [mini heat-map strip of all cells]                 │
│                           │  Temps 24.0–25.0 °C   [8 module dots]                                             │
├───────────────────────────┴─────────────────────────────────────────────────────────────────────────────────┤
│ Today ⇩ 19.2 kWh ⇧ 5.0 kWh   │  Inverter sees 98 % · CVL 382.4 V   │  Balance charge: floating, next in 7 days  │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

- **Hero:** the SoC ring, the system and contactor states as chips, and charge/discharge
  direction and power in large type.
- **"Limited by":** which current limiter is in force and its value, the single most useful
  diagnostic, which today needs HA.
- **Cells and temperatures as glanceable strips,** min/max/Δ in text, colour for outliers.
  Tap through to Cells.
- **Events banner:** present only when an event is active, coloured by severity, tap to open
  Events.
- **Bottom strip:** today's energy, what the inverter sees, balance-charge status.
- **Lives at `main_v2_page`** next to `energy_page`; nothing on the current main changes. When
  you've chosen, the other is deleted and the tab bar goes back to one Main.

## Design rules for 7" (1024×600)

- **Text:** values 28–40 px, labels 16–18 px, so it reads from arm's length. Nothing smaller than 14 px.
- **Touch targets:** at least 64×64 px for anything you tap, and 72 px tall for tabs and command buttons.
- **Colour:** black background with the CellKeeper mint accent (`#5ee3a8`) for selection and highlights.
  - **Status colours are reserved:** green OK, amber warning, red fault. They always come with a text label, never colour alone.
  - Cell colours use one sequential ramp, low to high, plus an outline for balancing.
- **Cards:**
  - 12 px radius, 16 px padding, two or three columns.
  - Every value has a label and a unit.
  - Stale data (no BMS reply for 5 s) greys out and shows "--".
- **Settings editing:** use the existing numeric pad, but show the current value, unit and allowed range, and refuse out-of-range input before it reaches the BMS. While locked, values show with a lock icon and taps do nothing.
- **Dangerous actions** (Stop, Run, calibrations, Wi-Fi reset, balance charge start) need **hold-to-confirm** (1.5 s) or a confirm dialog. Never a single tap.

## Implementation

1. **Split the layout into include files:**
   - `ui/1024x600/styles.yaml`, with LVGL `style_definitions` for cards, value text, label text, status chips and tab buttons
   - `ui/1024x600/header.yaml` and `tabbar.yaml`, as `top_layer` widgets
   - one file per page: `ui/1024x600/page_main.yaml` (moved unchanged), `page_cells.yaml`, `page_power.yaml`, `page_health.yaml`, `page_settings.yaml`, `page_system.yaml`

   `ui_1024x600.yaml` becomes the list of includes, so the hardware packages don't change.
2. **Use shared styles instead of repeating** fonts, colours and sizes on every label. That shrinks the YAML and makes the 800×480 port a matter of styles and sizes.
3. **Update only the visible page.** `bms_ui_scripts.yaml` already checks `lv_scr_act()`; extend that to the new pages, and keep each update to the labels on screen.
4. **Request only the registers the visible page needs.** Ask `hmi_bms` for the extra registers the Power and Health pages show only while those pages are open, so the 100 ms polling doesn't grow for everyone.
5. **Keep the IDs that HA and the scripts use** (`my_display`, `my_touchscreen`, `display_backlight`, `pad_overlay` …) so nothing outside the UI breaks.

## Phases (each one flashable and testable on its own)

1. **Skeleton:** styles, header, tab bar and empty pages, with main moved unchanged into its file. Check navigation and the main page are untouched.
2. **Events:** the three tiers from the HA Event view, and the header badge. It comes early
   because it's the most-wanted feature.
3. **Main v2:** the re-imagined main page next to the current one.
4. **Cells:** move `details_page` content and add the heat-map colouring, the module temperature
   strip and balancing status.
5. **Settings:** merge limits and balancing, add balance charge, with range checks, the write
   status and help pop-ups.
6. **Power:** limit breakdown, the inverter's view, voltages, SoC estimators, energy.
7. **Health:** isolation, supplies, temperatures, supervisor.
8. **System:** commands with confirmation, log viewer, network, about, raw registers.
9. **Clean-up:** keep whichever main you chose, delete the old pages and scripts, then port to
   `ui_800x480.yaml` (same pages, smaller fonts and grid), or drop 800×480 if the S3 7" isn't
   used.

**Testing:** build each phase for the desk **Waveshare 7B** (OTA to 10.60.14.129) first, then the desk Guition, then the shed and BlueTesla HMIs. Check every page with a real BMS attached, including stale-data behaviour (unplug the link) and the lock and confirm flows.

## Open questions

- **Tab bar on Main:** is a pull-up handle acceptable there, or should Main get a slim always-visible bar too? (Main's layout stays as it is either way.)
- **Auto-return timeout**, and whether the screen should dim or sleep when idle.
- **Settings PIN:** keep the simple unlock toggle, or require a PIN for the Settings and System pages?
- **800×480:** port it, or retire it?
- **Events on the LCD:** show all 61 types like HA, or only those that have ever been active?
- **Main v2 as the default page** while you compare, or keep the current main as the boot page?
- **Before phase 1:** do you want a clickable HTML mock-up of the new pages at 1024×600,
  including Main v2, to approve the layout?
