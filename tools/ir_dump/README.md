# ir_dump — scan the AC remote

A standalone sketch that finds out which IR protocol the air conditioner remote
(Delfa) uses. Use it once, before the IR control feature is built.

The HKL-EA8 has an IR receiver on **GPIO33** and an IR sender on **GPIO32**
(from the vendor demo code). This sketch only uses the receiver.

## Requirements

- Library **IRremoteESP8266** (2.9.0 compiles on ESP32 core 4.0.0):
  `arduino-cli lib install IRremoteESP8266`

## Steps

1. Flash the scanner:
   `arduino-cli upload --fqbn esp32:esp32:nodemcu-32s -p COMx tools/ir_dump`
2. Open the serial monitor:
   `arduino-cli monitor -p COMx -c baudrate=115200`
3. Point the remote at the board and press a few buttons, one at a time, for
   example: cool 24 °C, heat 22 °C, change the fan speed, off.
4. Save the serial output (it is needed to design the feature).
5. Flash the main firmware back:
   `arduino-cli upload --fqbn esp32:esp32:nodemcu-32s -p COMx .`

## Reading the output

- `Protocol: GREE` / `MIDEA` / ... plus an `AC state: ...` line means the
  library knows the protocol. The firmware can then build any command (power,
  mode, temperature, fan) and nothing needs to be recorded.
- `Protocol: UNKNOWN` means it does not. Then a few fixed commands are recorded
  from the remote as raw timings and replayed (fallback).

## How the firmware will send

The ACs are reached through board relays 2–5 (relay 1 is used by air cond),
one relay per AC. To send to one AC: close its relay, wait about 30 ms for the
contacts to settle, send the full state on GPIO32, open the relay. Only one
relay is closed at a time.

IR is one-way: the firmware only knows the last state it sent. A change made
with the handheld remote is not seen.
