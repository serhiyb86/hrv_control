# Cooper&Hunter CH-HRV3.5KDC — Modbus Investigation

---

## 2. Physical connection

- **RS485 terminal on the main board:** 4 pins, `12V | A | B | GND`

---

## 3. Communication settings

| Setting | Value |
|---|---|
| Protocol | Modbus RTU over RS485 | manual |
| Slave address | **14** |
| Baud rate |  **4800** | 
| Data format | **8N1** |
| FC | **3**|

**FC03 block reads (tested 2026-11-17 with the HRV controller's Raw register access):**

| Request | Registers | Result |
|---|---|---|
| Reg 2, count 1 | R2 | OK |
| Reg 2, count 20 | R2–R21 | OK |
| Reg 2, count 21 | R2–R22 | timeout (no reply) |
| Reg 3, count 21 | R3–R23 | timeout (no reply) |
| Reg 3, count 20 | R3–R22 | OK |
| Reg 22, count 1 | R22 | OK |

**Limit: at most 20 registers per FC03 request.** All registers 2–22 are readable, alone or in a block.
The unit does not send an exception for a request it rejects. It stays silent, so the master sees a timeout.
`hrv_control.ino` reads R2–R21 in one request every poll and R22 on its own every 30 s.

**Sample raw read (2026-11-17), reg 3 count 20:**

```
TX: 0E 03 00 03 00 14 B5 3A
RX: 0E 03 28 00 03 00 1E 00 2E 00 0A 00 66 00 0E 00 01 00 08 00 08 00 38 00 34 00 39 00 30 00 00 00 00 00 04 00 60 00 00 00 00 00 00 B7 D2
```

| Reg | Raw | Note |
|---|---|---|
| R3 | 3 | bypass band Y, matches default |
| R4 | 30 | defrost interval, matches default |
| R5 | **46** | expected −9…5 °C (default −1). Temperature encoding is not plain °C. |
| R6 | 10 | defrost duration, matches default |
| R7 | 102 (0x66) | CO₂ setpoint 1000 ppm, matches default |
| R8 | 14 | Modbus address |
| R9 | 1 | unit ON |
| R10 / R11 | 8 / 8 | fan speed code 8 = 4th speed |
| R12 | **56** | RA return air: 56 − 40 = 16 °C |
| R13 | **52** | OA outside air: 12 °C |
| R14 | **57** | SA supply air: 17 °C |
| R15 | **48** | FR fresh/exhaust air: 8 °C |
| R16 / R17 | 0 / 0 | external on/off, CO₂ signal |
| R18 | 4 | B2 = bypass off |
| R19 | **96** | heater stage: unexpected value |
| R20 | 0 | no errors |
| R21 | **0** | model selection: doc says 4 for 3.5KDC |
| R22 | 0 | defrost mode |

**Open questions:**
- **Temperature encoding** for R12–R15: resolved, °C = raw − 40. R5 is still open (raw − 47 would make it the default −1).
- **R19 = 96 and R21 = 0** do not match the register map.


---

## 4. Modbus register map (p.21)

| Reg | Meaning | Range / encoding | Default |
|---|---|---|---|
| 00 | Not used | | |
| 01 | Not used | | |
| 02 | Bypass opening temperature X | 5–30 °C | 19 |
| 03 | Bypass temperature band Y | 2–15 °C | 3 |
| 04 | Defrost interval | 15–99 min | 30 |
| 05 | Outdoor temperature that triggers defrost | −9…5 °C | −1 |
| 06 | Defrost duration | 2–20 min | 10 |
| 07 | CO₂ setpoint | 0x28–0xC8 = 392–1960 ppm (ppm ≈ raw × 9.8) | 0x66 = 1000 ppm |
| 08 | Modbus address | 1–16 | 1 |
| 09 | Unit on/off | 0 = OFF, 1 = ON | |
| 10 | Supply fan speed | see fan-speed codes below | |
| 11 | Exhaust fan speed | see fan-speed codes below | |
| **12** | **RA — return air** (extracted from the building), °C = raw − 40 | current value (read only) | |
| 13 | OA — outside air (outdoor air entering the unit), °C = raw − 40 | current value | |
| 14 | SA — supply air (supplied to the building), °C = raw − 40 | current value | |
| 15 | FR — fresh/exhaust air (discharged outdoors), °C = raw − 40 | current value | |
| 16 | External on/off input | status | |
| 17 | CO₂ on/off signal | status | |
| 18 | Status bits | B0 fire alarm, B1 bypass on, B2 bypass off, B3 defrosting | |
| 19 | Electric heater stage | | |
| 20 | Error bits | B2 outdoor sensor, B3 defrost sensor, B4 room sensor, B5 EEPROM | |
| 21 | Model selection | 0–7 (3.5KDC = 4) | |
| 22 | Defrost mode | | |

**Fan-speed codes (registers 10 and 11):**

| Code | Speed | Code | Speed |
|---|---|---|---|
| 0 | Stop | 10 | 6th |
| 2 | 1st | 11 | 7th |
| 3 | 2nd | 12 | 8th |
| 5 | 3rd | 13 | 9th |
| 8 | 4th | 14 | 10th |
| 9 | 5th | | |

Codes 1, 4, 6 and 7 are not listed.

**Bypass logic (p.19):** the bypass opens when X ≤ outdoor temperature < X + Y. With the defaults, that is 19–22 °C.

---

## 5. Additional functionality relying on controller

| No. | Meaning | Range | Default |
|---|---|---|---|
| 01 | Auto-restart after power loss | on, off | on |
| 02 | Fan control depending on outside and room temperature | to find out how to controll | 2 |
| 03 | TBD | |

## 6. Additional info to store on the controller page
create a link "manual" with such info


#### DIP switch SW4 (p.20) — turn the power off before changing

| Switch | OFF | ON |
|---|---|---|
| SW4-1 | Standard defrost using the exhaust fan | Defrost using an electric heater on the outdoor-air side (parameter 01 is forced to 0) |
| SW4-2 | Automatic bypass | Manual bypass through a dry contact (free cooling); fans run at high speed |
| SW4-3 | CO₂ sensor | Humidity/temperature sensor (error E3 if the sensor is missing) |
| SW4-4 | Not used | Not used |

#### Display error codes (p.19)

| Code | Error |
|---|---|
| E1 | Outdoor air temperature sensor |
| E2 | EEPROM |
| E3 | Exhaust air temperature sensor |
| E4 | Defrost sensor |
| E5 | Communication |
| E6 | Supply air temperature sensor |
| E7 | Exhaust fan |
| E8 | Supply fan |

---
### CH-HRV3.5KDC specifications (p.5)

| Item | Value |
|---|---|
| Airflow | 350 m³/h (100 l/s) |
| Enthalpy-exchange efficiency | 69 % heating / 66 % cooling |
| Temperature-exchange efficiency | 75 % |
| Sound pressure | 28 dB(A) |
| Power supply | 220 V / 1 ph / 50 Hz |
| Power consumption | 112 W |
| Power cable / control cable | 2×1.5 mm² / 2×0.5 mm² |
| Duct diameter | Ø150 mm |
| Dimensions (W × H × L) | 804 × 270 × 882 mm |
| Weight | 37 kg |
| Fans | DC motors, 10 speeds each (supply and exhaust) |
| Control | Wired controller with weekly timer; BMS via Modbus |
| Summer bypass / defrost | Automatic, adjustable range |
| CO₂ control | Optional extra controller |
| Inputs | External on/off; fire alarm (1 dry contact, closed = off) |
| Model code (parameter 21) | **4** |

---
