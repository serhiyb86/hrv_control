# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ESP32 firmware (single Arduino sketch) that controls a Cooper&Hunter CH-HRV3.5KDC heat-recovery ventilator over Modbus RTU / RS485 and serves a web UI at `http://192.168.1.200/` (static IP, Ethernet only, no Wi-Fi).

Hardware: HKL-EA8 board (ESP32 + LAN8720 Ethernet + PCF8574 relay expander at 0x24 on I2C SDA 4 / SCL 5 + RS485 on RX 14 / TX 13, auto-direction transceiver). In Arduino IDE it is **NodeMCU-32S**. Vendor example code: https://github.com/hankerila-MA/HKL-EA8

## Workflow

- For any change that affects the UI or UX, update `mockup.html` first (with simulated data) and **stop until the user approves it**. Only then port the change to `page.h` and add any firmware/API changes in `hrv_control.ino`.
- Firmware-only changes (Modbus, storage, logic with no UI impact) don't need a mockup update.
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
- **Temperatures R12–R15 are °C = raw − 40** (`TEMP_OFS`, applied in `tempOf()`): R12 RA return air (from the rooms), R13 OA outside air, R14 SA supply air (into the rooms), R15 FR fresh/exhaust air (out to outdoors). Other temperature-like registers (R2, R3, R5) are not decoded this way. `Config.unused1` is the old `tempDiv` byte, kept so the NVS blob layout stays valid.

**State model**:
- `regs[]`/`regOk[]` — last polled register values. After 3 failed polls in a row every `regOk` is cleared, so check `regOk` before using a value.
- `des` (`Desired`: on, supply speed, exhaust speed) is the state the user wants. It is set by web actions and the timer, and by `adoptState()` when someone changes the wall controller (ignored for 8 s after our own writes). After the link comes back (`hadFailure`), `restoreTick()` re-applies `des` once `cfg.restoreDelay` has passed. This is the "auto-restart after power loss" feature.
- Writes always go through `setPower()` / `setSpeeds()` / `writeReg()`. These update `regs`, `des` and the log, and set `ownWriteMs`. Don't call `mbWrite` directly for control actions.
- Temperature rules (`evalRules`): the first enabled rule that matches wins. The active rule is kept while it still matches with hysteresis `cfg.hyst`, and switches are held for at least `cfg.holdMin`. In a rule, a `NAN` field means "any".
- Weekly timer: 2 periods per day, minutes since midnight, day index Mon=0. A manual change while the timer is on sets `manualMs`; `cfg.revertMin` then reverts to the schedule.

**Persistence**:
- `Config cfg` and `des` are stored as raw struct blobs in NVS (`Preferences`, namespace `"hrv"`, keys `"cfg"`/`"des"`). On load the blob size must equal `sizeof(Config)` and `magic` must equal `CFG_MAGIC`, otherwise defaults are used. **Any change to the `Config`/`Rule`/`Period` layout silently resets all user settings.** Bump `CFG_MAGIC` deliberately, or add migration code.
- History lives in LittleFS as packed, fixed-size records sorted by time (`MinRec`/`HourRec`/`DayRec`): `/m/YYYYMMDD.bin` (minutes, 7 days), `/h/YYYYMM.bin` (hours, 24 months), `/d/YYYY.bin` (days, 10 years). Readers binary-search on the leading `uint32_t t`. Changing a record struct breaks existing files. Temperatures are stored ×10 as `int16`, with `NOVAL` meaning no value. History is only recorded once NTP time is valid (`timeValid()`).
- Energy (`energySample`), supply side: kept = 0.335 × supply flow × (SA − OA), lost = 0.335 × supply flow × (RA − SA), flows from `cfg.flow[level]` (m³/h); fan power from `cfg.fanW[level]`. History record fields map as room = RA, out = OA, exh = FR, sup = SA.

**Web API**: routes are registered in `setup()`. JSON is built by hand with `String` concatenation (`jsonEsc`, `jNum` — NaN becomes `null`). POST endpoints read form args via `server.arg()`. `/api/config` POST takes a `section` arg (`link`, `rst`, `rules`, `tmr`, `en`). `/relays` + `/relay` is a separate plain-HTML page for the 8 board relays (not used by the HRV logic yet).

Time zone is Europe/Kyiv (`TZ_INFO`). NTP servers include an IP literal so time works without DNS.

## Planned features (from README)

1. Use the bypass as air conditioning
2. Control 5 air conditioners over IR
3. Basement ventilation
4. Telegram bot
