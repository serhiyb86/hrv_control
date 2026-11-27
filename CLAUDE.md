# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ESP32 firmware (single Arduino sketch) that controls a Cooper&Hunter CH-HRV3.5KDC heat-recovery ventilator over Modbus RTU / RS485 and serves a web UI at `http://192.168.1.200/` (static IP, Ethernet only, no Wi-Fi).

Hardware: HKL-EA8 board (ESP32 + LAN8720 Ethernet + PCF8574 relay expander at 0x24 on I2C SDA 4 / SCL 5 + RS485 on RX 14 / TX 13, auto-direction transceiver). In Arduino IDE it is **NodeMCU-32S**. Vendor example code: https://github.com/hankerila-MA/HKL-EA8

## Workflow

- For any change that affects the UI or UX, update `mockup.html` first (with simulated data) and **stop until the user approves it**. Only then port the change to `page.h` and add any firmware/API changes in `hrv_control.ino`.
- Firmware-only changes (Modbus, storage, logic with no UI impact) don't need a mockup update.
- GitHub issues: `.mcp.json` registers GitHub's remote MCP server (issues toolset only). It reads the token from the `GITHUB_HRV_TOKEN` environment variable (fine-grained PAT, this repo only, Issues read/write). Never put the token in the file. Use it to read, create and edit issues; the `gh` CLI on this machine is logged in to a different account without access.
- Git: commit directly to `main` and push to `origin` after each finished change (no feature branches or PRs). The remote is `git@github-hrv:serhiyb86/hrv_control.git`; `github-hrv` is an SSH host alias from `~/.ssh/config` that selects the right key. Don't rewrite the URL to `github.com`.

## Build

Requires Arduino-ESP32 core **3.x** and the `PCF8574 library` (Renzo Mischianti — the `PCF8574(addr, sda, scl)` constructor and `bool begin()` come from it). There are no tests and no linter; "compiles cleanly" is the only automated check.

```bash
arduino-cli compile --fqbn esp32:esp32:nodemcu-32s .
arduino-cli upload  --fqbn esp32:esp32:nodemcu-32s -p COMx .   # USB; there is no OTA
arduino-cli monitor -p COMx -c baudrate=115200                  # addLog() also prints to Serial
```

Flashing and verifying on the real unit is done by the user. Do not write Modbus registers on the live unit (via `/api/write`, `/api/raw`, `/api/addr`, etc.) without asking.

## Files

- `hrv_control.ino` — all firmware logic, split into `// ====== <section>` blocks (hardware, registers, types, globals, config, Modbus RTU master, unit state, temperature rules, weekly timer, history storage, energy, polling, web handlers, setup/loop). All types are declared at the top, before any function, to avoid Arduino's auto-prototype problems — keep new structs there.
- `page.h` — the production web UI: one HTML/CSS/JS page in a `PROGMEM` raw string (`R"HTMLPAGE(...)HTMLPAGE"`), served by `handleRoot`. It talks to the device only through the `/api/*` JSON endpoints. Edit it by hand. Never put the sequence `)HTMLPAGE"` inside it.
- `mockup.html` — standalone UI mockup; open it straight from disk in a browser. It is the `page.h` page verbatim, plus three mockup-only additions: the `(MOCKUP)` title, the purple `.mock` badge, and a `MOCK BACKEND` `<script>` block. That block overrides `fetch('/api/...')` and answers from a simulated unit using the same JSON formats as the firmware handlers. UI changes start here (see Workflow). Port everything outside the mockup-only parts to `page.h` unchanged. When an `/api` response format changes, update the mock backend too.
- `docs/CH-HRV3.5KDC_modbus_investigation.md` — register map, fan-speed codes, link test results and **open questions**. `docs/cooperhunter-ch-hrv3-5kdc-manual.pdf` is the unit manual.

## Architecture

**Main loop** (`loop()`): `server.handleClient()` → `pollUnit()` every `cfg.pollSec` (or right away when `pollNow` is set, e.g. after a write) → `restoreTick()` → `minuteTick()` (finishes minute records, rolls up history, runs the weekly timer) → `saveDesiredIfDirty()`. Everything is single-threaded and blocking; a Modbus transaction blocks the web server for up to `cfg.timeoutMs`.

**Modbus link facts** (they explain non-obvious code):
- Slave 14, 4800 8N1. Reads use FC03; writes use FC06 by default, FC16 can be selected (`cfg.writeFc`).
- The unit answers **at most 20 registers per FC03 request**. Asking for more gets silence, not an exception. So `pollUnit` reads R2–R21 as one block, and R0, R1 and R22 one by one every 30 s. If the block read fails but a single read of R12 works, it switches to `singleMode` (per-register reads, with a 60 s skip for registers that keep failing).
- `mbTransact` handles transceivers that echo the request (`echoSeen`). An FC06 reply is byte-identical to the request, hence the `sameAsReq` logic.
- Fan speed registers (R10/R11) hold **codes**, not levels: `SPEED_CODE[level]` with levels 0–10; use `levelOf()` for the reverse mapping.
- **Temperatures R12–R15 are °C = raw − 40** (`TEMP_OFS`, applied in `tempOf()`): R12 RA return air (from the rooms), R13 OA exhaust air (out to outdoors), R14 SA supply air (into the rooms), R15 FR fresh air (intake from outdoors). **FR is the outdoor temperature** (checked on the unit), so rules, energy and air cond use R15, not R13. Other temperature-like registers (R2, R3, R5) are not decoded this way. `Config.unused1` is the old `tempDiv` byte, kept so the NVS blob layout stays valid.

**State model**:
- `regs[]`/`regOk[]` — last polled register values. After 3 failed polls in a row every `regOk` is cleared, so check `regOk` before using a value.
- `des` (`Desired`: on, supply speed, exhaust speed) is the state the user wants. It is set by web actions and the timer, and by `adoptState()` when someone changes the wall controller (ignored for 8 s after our own writes). After the link comes back (`hadFailure`), `restoreTick()` re-applies `des` once `cfg.restoreDelay` has passed. This is the "auto-restart after power loss" feature.
- Writes always go through `setPower()` / `setSpeeds()` / `writeReg()`. These update `regs`, `des` and the log, and set `ownWriteMs`. Don't call `mbWrite` directly for control actions.
- Temperature rules (`evalRules`): the first enabled rule that matches wins. The active rule is kept while it still matches with hysteresis `cfg.hyst`, and switches are held for at least `cfg.holdMin`. In a rule, a `NAN` field means "any".
- Weekly timer: 2 periods per day, minutes since midnight, day index Mon=0. `off < on` means the period runs past midnight and turns off on the next day (`off == on` is rejected). A manual change while the timer is on sets `manualMs`; `cfg.revertMin` then reverts to the schedule.
- Air cond (`acTick`, after every poll): free cooling by bypass. Board relay 1 (`AC_RELAY`) drives the unit's manual-bypass dry contact (DIP SW4-2 = ON; the unit then runs the fans at high speed). ON when RA > `ac.tgt` and FR ≤ RA − `ac.d`; OFF when RA ≤ tgt − hyst or FR > RA − d + hyst; `ac.hold` minutes between switches, optional allowed hours. It wins over the timer: `timerSwitch(false)` is skipped while the relay is closed, and `acStop()` hands back (unit off if `timerWants()` says off, or if air cond switched it on). Relay opens on sensor error, link loss and boot (PCF8574 starts off). A manual OFF (web or wall controller) sets `acManOff` until the next timer ON or manual ON. Relay 1 is locked on `/relays` while enabled.

**Persistence**:
- `Config cfg` and `des` are stored as raw struct blobs in NVS (`Preferences`, namespace `"hrv"`, keys `"cfg"`/`"des"`). On load the blob size must equal `sizeof(Config)` and `magic` must equal `CFG_MAGIC`, otherwise defaults are used. **Any change to the `Config`/`Rule`/`Period` layout silently resets all user settings.** Bump `CFG_MAGIC` deliberately, or add migration code. New feature settings can go under their own key instead, like `AcCfg ac` (key `"ac"`, `AC_MAGIC`), which leaves `cfg` untouched.
- History lives in LittleFS as packed, fixed-size records sorted by time (`MinRec`/`HourRec`/`DayRec`): `/m/YYYYMMDD.bin` (minutes, 7 days), `/h/YYYYMM.bin` (hours, 24 months), `/d/YYYY.bin` (days, 10 years). Readers binary-search on the leading `uint32_t t`. Changing a record struct breaks existing files. Temperatures are stored ×10 as `int16`, with `NOVAL` meaning no value. History is only recorded once NTP time is valid (`timeValid()`) and only while the unit is on, so the charts show gaps for off periods. `eachMin` also skips older minute records with the unit-on flag (bit 0) clear; hour/day records written before 2026-11-27 still include off periods.
- Energy (`energySample`), supply side: kept = 0.335 × supply flow × (SA − FR), lost = 0.335 × supply flow × (RA − SA), flows from `cfg.flow[level]` (m³/h); fan power from `cfg.fanW[level]`. Free cooling (`live.cool`, live only, not in history) = 0.335 × supply flow × (RA − FR) while R18 B1 reports the bypass open. History record fields map as room = RA, out = FR (outdoor), exh = OA (exhaust), sup = SA. Records written before 2026-11-26 have out/exh the other way round.

**Web API**: routes are registered in `setup()`. JSON is built by hand with `String` concatenation (`jsonEsc`, `jNum` — NaN becomes `null`). POST endpoints read form args via `server.arg()`. `/api/config` POST takes a `section` arg (`link`, `rst`, `rules`, `tmr`, `en`, `ac`). `/relays` + `/relay` is a separate plain-HTML page for the 8 board relays; relay 1 is used by air cond. Write relays through `setRelay()`. The relays are **active-low** (`RELAY_ON` = LOW, checked on the board, unlike the vendor demo); set `pinMode(ch, OUTPUT, RELAY_OFF)` before `relays.begin()`.

Time zone is Europe/Kyiv (`TZ_INFO`). NTP servers include an IP literal so time works without DNS.

## Planned features (from README)

1. ~~Use the bypass as air conditioning~~ — done (air cond mode, issue #2)
2. Control 5 air conditioners over IR
3. Basement ventilation
4. Telegram bot
