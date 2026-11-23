// HRV controller for Cooper&Hunter CH-HRV3.5KDC over Modbus RTU (RS485)
// Board: HKL-EA8 (ESP32 + LAN8720 Ethernet + PCF8574 relays + RS485), Arduino-ESP32 core 3.x
//
// Web UI: http://192.168.1.200/
//   Dashboard     - temperatures, power, fan speeds, status and error bits
//   Unit settings - bypass, defrost, CO2 setpoint (registers 2..7), Modbus address
//   Automation    - auto-restart after power loss, fan rules by temperature, weekly timer
//   Energy        - heat kept / lost, history charts, CSV, clear history
//   Advanced      - link settings, raw register access, log
//
// Link (see CH-HRV3.5KDC_modbus_investigation.md): slave 14, 4800 8N1, FC03 reads.
// Writes use FC06 by default (FC16 can be selected in Advanced).
//
// History is kept in LittleFS (partition "spiffs", default 4 MB layout gives ~1.4 MB):
//   /m/YYYYMMDD.bin  one record per minute, kept 7 days
//   /h/YYYYMM.bin    one record per hour,   kept 24 months
//   /d/YYYY.bin      one record per day,    kept 10 years

#include <Arduino.h>
#include <ETH.h>
#include <WebServer.h>
#include <PCF8574.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>
#include <vector>
#include <algorithm>
#include <functional>
#include "page.h"

// ============================================================ hardware
#define ETH_ADDR       0
#define ETH_POWER_PIN -1
#define ETH_MDC_PIN   23
#define ETH_MDIO_PIN  18
#define ETH_TYPE      ETH_PHY_LAN8720
#define ETH_CLK_MODE  ETH_CLOCK_GPIO17_OUT

IPAddress local_IP(192, 168, 1, 200);
IPAddress gateway (192, 168, 1, 1);
IPAddress subnet  (255, 255, 255, 0);
IPAddress dns1    (192, 168, 1, 1);
IPAddress dns2    (8, 8, 8, 8);

#define SDA_PIN 4
#define SCL_PIN 5
PCF8574 relays(0x24, SDA_PIN, SCL_PIN);
bool relayState[8] = {false};

#define RS485_RX 14
#define RS485_TX 13
#define RS485_DE -1   // GPIO for manual DE/RE, -1 = auto-direction transceiver

const char *TZ_INFO = "EET-2EEST,M3.5.0/3,M10.5.0/4";   // Europe/Kyiv
const char *NTP1 = "pool.ntp.org";
const char *NTP2 = "time.google.com";
const char *NTP3 = "162.159.200.1";    // time.cloudflare.com by IP: works without DNS

// ============================================================ registers
enum {
  R_RELOAD = 0, R_HEATCON = 1,   // automatic reloading, heater connection (not in the manual's map; read every 30 s)
  R_BYP_X = 2, R_BYP_Y = 3, R_DEF_INT = 4, R_DEF_OUT = 5, R_DEF_DUR = 6, R_CO2 = 7,
  R_ADDR = 8, R_POWER = 9, R_SUP = 10, R_EXH = 11, R_RA = 12, R_OA = 13, R_SA = 14,
  R_FR = 15, R_EXTIN = 16, R_CO2SIG = 17, R_STATUS = 18, R_HEATER = 19, R_ERR = 20,
  R_MODEL = 21, R_DEFMODE = 22
};
// R12..R15 temperatures, raw value = °C + 40:
//   RA return air (from the rooms), OA outside air, SA supply air (into the rooms), FR fresh/exhaust air (out to outdoors)
const int TEMP_OFS = 40;
const int REG_FIRST = 2, REG_LAST = 22, NREG = 23;
const int REG_BLOCK_LAST = 21;   // unit answers at most 20 registers per FC03 request (count 21 times out)
const uint16_t SPEED_CODE[11] = {0, 2, 3, 5, 8, 9, 10, 11, 12, 13, 14};  // index = speed level

const uint8_t ST_FIRE = 1, ST_BYPASS_ON = 2, ST_BYPASS_OFF = 4, ST_DEFROST = 8;
const uint16_t ERR_OUT = 4, ERR_DEF = 8, ERR_ROOM = 16, ERR_EEPROM = 32;

// ============================================================ types (declared before any function)
const uint32_t CFG_MAGIC = 0x48525632;
const int MAX_RULES = 8;

struct Rule {            // a field set to NAN means "any"
  uint8_t en;
  float outMin, outMax;  // outMin <= outdoor < outMax
  float roomMin, roomMax;
  uint8_t cmp;           // 0 any, 1 outdoor cooler than room by >= d, 2 outdoor warmer than room
  float d;
  uint8_t sup, exh;      // speed levels 0..10
};

struct Period { uint8_t en; uint16_t on, off; uint8_t sp; };  // minutes of the day; sp is unused (speed comes from the Dashboard)

struct Config {
  uint32_t magic;
  uint8_t addr; uint32_t baud; uint8_t fmt; uint16_t pollSec, timeoutMs; uint8_t unused1, writeFc;  // unused1 was tempDiv, kept so saved settings stay valid
  uint8_t autoRestart; uint16_t restoreDelay;
  uint8_t rulesOn; float hyst; uint16_t holdMin; uint8_t fallback; uint8_t nRules; Rule rules[MAX_RULES];
  uint8_t timerOn; uint16_t revertMin; Period tmr[7][2];
  float heatPrice, elPrice; uint16_t flow[11], fanW[11];
};

struct Desired { uint8_t valid, on, sup, exh; };

enum MbErr { MB_OK, MB_TIMEOUT, MB_CRC, MB_EXCEPTION, MB_BAD };

// History records. Temperatures are x10 °C, NOVAL = no value.
const int16_t NOVAL = INT16_MIN;
struct __attribute__((packed)) MinRec  { uint32_t t; int16_t room, out, exh, sup; uint16_t kept, lost, fan; uint8_t flags, spd; }; // kept/lost 0.1 Wh, fan 0.01 Wh
struct __attribute__((packed)) HourRec { uint32_t t; int16_t room, out, exh, sup; uint16_t kept, lost, fan, n; };              // Wh, Wh, 0.1 Wh, minutes
struct __attribute__((packed)) DayRec  { uint32_t t; int16_t room, out, exh, sup; uint32_t kept, lost, fan; uint16_t n; };      // Wh, Wh, 0.1 Wh, minutes

struct Sample { uint32_t t; float room, out, exh, sup, w, kept, lost, fan; };  // room = RA, out = OA, exh = FR, sup = SA; w = minutes, energy in Wh

struct Agg {
  double room = 0, out = 0, exh = 0, sup = 0, w = 0, exhW = 0, supW = 0, kept = 0, lost = 0, fan = 0;
  void add(const Sample &s) {
    if (s.w <= 0) return;
    room += s.room * s.w; out += s.out * s.w;
    if (!isnan(s.sup)) { sup += s.sup * s.w; supW += s.w; }
    if (!isnan(s.exh)) { exh += s.exh * s.w; exhW += s.w; }
    w += s.w; kept += s.kept; lost += s.lost; fan += s.fan;
  }
  Sample get(uint32_t t) const {
    Sample s = {t, NAN, NAN, NAN, NAN, 0, (float)kept, (float)lost, (float)fan};
    if (w > 0) { s.room = room / w; s.out = out / w; s.w = w; }
    if (supW > 0) s.sup = sup / supW;
    if (exhW > 0) s.exh = exh / exhW;
    return s;
  }
};

struct MinAcc { uint32_t t; double room, out, exh, sup, kept, lost, fan; uint16_t n, exhN; uint8_t flags, spd; };

struct Live { float kept, lost, fan; bool heating; };

typedef std::function<void(const Sample &)> SampleCb;
typedef std::function<void(const uint8_t *)> RecCb;

// ============================================================ globals
WebServer server(80);
Preferences prefs;
Config cfg;
Desired des = {0, 0, 5, 5};
bool desDirty = false; uint32_t desDirtyMs = 0;

uint16_t regs[NREG]; bool regOk[NREG];
uint8_t regFails[NREG]; uint32_t regSkipUntil[NREG];
uint32_t lastSlowMs = 0;                               // single-register mode: last read of the settings registers
bool linkOk = false, hadFailure = true, singleMode = false, pollNow = true;
uint32_t lastPollMs = 0, lastOkMs = 0; int failStreak = 0;

uint8_t mbEx = 0; bool echoSeen = false;
uint32_t mbOkCnt = 0, mbFailCnt = 0, mbLastErrMs = 0;
String mbLastErr = "-", mbTxHex, mbRxHex;

uint32_t ownWriteMs = 0, manualMs = 0;
bool restorePending = false; uint32_t restoreAtMs = 0; uint32_t lastRestore = 0;

const int RULE_UNSET = -2;
int activeRule = RULE_UNSET; uint32_t ruleSwitchMs = 0;
int lastTimerKey = -1;
String timerLast;   // last timer action, shown in the UI

bool fsOk = false;
MinAcc macc = {};
MinRec minBuf[16]; int minBufN = 0;
uint32_t lastHourDone = 0, lastDayDone = 0; bool histInit = false;
uint32_t lastEnergyMs = 0;
Live live = {0, 0, 0, NAN, false};
bool timeWasValid = false;

const int LOG_N = 60;
String logLines[LOG_N]; int logHead = 0, logCount = 0;

const uint32_t FMT_CFG[] = {SERIAL_8N1, SERIAL_8E1, SERIAL_8O1, SERIAL_8N2};
const char *FMT_NAME[] = {"8N1", "8E1", "8O1", "8N2"};
const char *MB_ERR_NAME[] = {"ok", "timeout", "bad CRC", "exception", "unexpected reply"};

// ============================================================ small helpers
bool timeValid() { return time(nullptr) > 1700000000; }
uint32_t nowT() { return (uint32_t)time(nullptr); }

void addLog(const char *fmt, ...) {
  char msg[200], line[240];
  va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
  if (timeValid()) {
    time_t t = time(nullptr); struct tm tm; localtime_r(&t, &tm);
    snprintf(line, sizeof line, "%02d.%02d %02d:%02d:%02d  %s", tm.tm_mday, tm.tm_mon + 1, tm.tm_hour, tm.tm_min, tm.tm_sec, msg);
  } else {
    snprintf(line, sizeof line, "+%lus  %s", (unsigned long)(millis() / 1000), msg);
  }
  Serial.println(line);
  logLines[logHead] = line;
  logHead = (logHead + 1) % LOG_N;
  if (logCount < LOG_N) logCount++;
}

String hexStr(const uint8_t *b, int n) {
  String s; char t[4];
  for (int i = 0; i < n && i < 64; i++) { snprintf(t, sizeof t, "%02X ", b[i]); s += t; }
  if (n > 64) s += "...";
  return s;
}

String jsonEsc(const String &s) {
  String o; o.reserve(s.length() + 8);
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c >= 32) o += c;
  }
  return o;
}

void jNum(String &j, float v, int dec) { if (isnan(v)) j += "null"; else j += String(v, dec); }

int levelOf(uint16_t code) { for (int i = 0; i < 11; i++) if (SPEED_CODE[i] == code) return i; return -1; }

int16_t toI16(float v) { return isnan(v) ? NOVAL : (int16_t)lroundf(v * 10); }
float fromI16(int16_t v) { return v == NOVAL ? NAN : v / 10.0f; }

float parseF(const String &s) { String t = s; t.trim(); return t.length() ? t.toFloat() : NAN; }

int splitCsv(const String &s, String *out, int maxn) {   // keeps empty fields
  int n = 0, p = 0;
  while (n < maxn) {
    int q = s.indexOf(',', p);
    if (q < 0) { out[n++] = s.substring(p); break; }
    out[n++] = s.substring(p, q);
    p = q + 1;
  }
  return n;
}

// ---- local calendar helpers
uint32_t mkLocal(struct tm &tm) { tm.tm_isdst = -1; return (uint32_t)mktime(&tm); }
uint32_t dayStartOf(uint32_t t) { time_t x = t; struct tm tm; localtime_r(&x, &tm); tm.tm_hour = tm.tm_min = tm.tm_sec = 0; return mkLocal(tm); }
uint32_t nextDay(uint32_t d) { return dayStartOf(d + 26 * 3600); }
uint32_t prevDay(uint32_t d) { return dayStartOf(d - 3600); }
uint32_t monthStartOf(uint32_t t) { time_t x = t; struct tm tm; localtime_r(&x, &tm); tm.tm_mday = 1; tm.tm_hour = tm.tm_min = tm.tm_sec = 0; return mkLocal(tm); }
uint32_t nextMonth(uint32_t m) { return monthStartOf(m + 32 * 86400UL); }
uint32_t prevMonth(uint32_t m) { return monthStartOf(m - 3600); }
uint32_t yearStartOf(uint32_t t) { time_t x = t; struct tm tm; localtime_r(&x, &tm); tm.tm_mon = 0; tm.tm_mday = 1; tm.tm_hour = tm.tm_min = tm.tm_sec = 0; return mkLocal(tm); }
uint32_t nextYear(uint32_t y) { return yearStartOf(y + 370 * 86400UL); }
void localTm(uint32_t t, struct tm &tm) { time_t x = t; localtime_r(&x, &tm); }

// ============================================================ config
void setDefaults() {
  memset(&cfg, 0, sizeof cfg);
  cfg.magic = CFG_MAGIC;
  cfg.addr = 14; cfg.baud = 4800; cfg.fmt = 0; cfg.pollSec = 2; cfg.timeoutMs = 300; cfg.unused1 = 0; cfg.writeFc = 6;
  cfg.autoRestart = 1; cfg.restoreDelay = 10;
  cfg.rulesOn = 0; cfg.hyst = 1; cfg.holdMin = 5; cfg.fallback = 0;
  const Rule def[] = {
    {1, NAN, -15, NAN, NAN, 0, 3, 2, 3},
    {1, NAN, -5,  NAN, NAN, 0, 3, 3, 4},
    {1, NAN, NAN, 25,  NAN, 1, 3, 8, 8},
    {1, NAN, NAN, 24,  NAN, 2, 0, 2, 2},
    {1, NAN, NAN, NAN, NAN, 0, 3, 5, 5},
  };
  cfg.nRules = 5;
  memcpy(cfg.rules, def, sizeof def);
  cfg.timerOn = 0; cfg.revertMin = 0;
  for (int d = 0; d < 7; d++) {
    if (d < 5) { cfg.tmr[d][0] = {1, 390, 540, 5}; cfg.tmr[d][1] = {1, 1020, 1380, 5}; }
    else       { cfg.tmr[d][0] = {1, 480, 1410, 5}; cfg.tmr[d][1] = {0, 0, 0, 3}; }
  }
  cfg.heatPrice = 4.32f; cfg.elPrice = 4.32f;
  const uint16_t fw[11] = {0, 8, 11, 15, 20, 27, 36, 48, 63, 85, 112};
  for (int i = 0; i < 11; i++) { cfg.flow[i] = 35 * i; cfg.fanW[i] = fw[i]; }
}

void loadConfig() {
  prefs.begin("hrv", false);
  if (prefs.getBytesLength("cfg") == sizeof cfg) {
    prefs.getBytes("cfg", &cfg, sizeof cfg);
    if (cfg.magic != CFG_MAGIC) setDefaults();
  } else {
    setDefaults();
  }
  if (prefs.getBytesLength("des") == sizeof des) prefs.getBytes("des", &des, sizeof des);
}

void saveConfig() { prefs.putBytes("cfg", &cfg, sizeof cfg); }

void saveDesiredIfDirty() {
  if (desDirty && millis() - desDirtyMs > 30000) {
    prefs.putBytes("des", &des, sizeof des);
    desDirty = false;
  }
}

// ============================================================ Modbus RTU master
uint16_t crc16(const uint8_t *d, int len) {
  uint16_t crc = 0xFFFF;
  for (int p = 0; p < len; p++) {
    crc ^= d[p];
    for (int i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

uint32_t gapMs() { uint32_t t = (uint32_t)(3.5 * 11 * 1000.0 / cfg.baud) + 2; return t < 4 ? 4 : t; }

void uartApply() {
  Serial1.end();
  delay(5);
  Serial1.setRxBufferSize(512);
  Serial1.begin(cfg.baud, FMT_CFG[cfg.fmt], RS485_RX, RS485_TX);
  delay(5);
  while (Serial1.available()) Serial1.read();
}

// req must have room for 2 CRC bytes after len. Handles transceivers that echo the request.
MbErr mbTransact(uint8_t *req, int len, uint8_t *rx, int rxMax, int expectLen, int &n) {
  uint16_t c = crc16(req, len);
  req[len++] = c & 0xFF; req[len++] = c >> 8;
  const bool sameAsReq = (expectLen == len);   // FC06 reply is identical to the request

  delay(gapMs());
  while (Serial1.available()) Serial1.read();
  if (RS485_DE >= 0) digitalWrite(RS485_DE, HIGH);
  Serial1.write(req, len);
  Serial1.flush();
  if (RS485_DE >= 0) digitalWrite(RS485_DE, LOW);

  n = 0;
  uint32_t start = millis(), last = start, gap = gapMs();
  while (millis() - start < cfg.timeoutMs) {
    while (Serial1.available() && n < rxMax) { rx[n++] = Serial1.read(); last = millis(); }
    if (n > 0) {
      bool echo = n >= len && memcmp(rx, req, len) == 0 && (!sameAsReq || echoSeen || n > len);
      int m = n - (echo ? len : 0);          // reply bytes received so far
      uint32_t quiet = millis() - last;
      if (m >= expectLen && quiet >= gap) break;                       // complete reply
      if (m == 5 && (rx[n - 4] & 0x80) && quiet >= gap) break;         // exception reply
      if (m > 0 && quiet >= 60) break;                                 // incomplete reply, unit went silent
    }
    delay(1);
  }
  if (n >= len && memcmp(rx, req, len) == 0) {
    if (!sameAsReq || n >= 2 * len) { memmove(rx, rx + len, n - len); n -= len; echoSeen = true; }
    else if (echoSeen) n = 0;    // only our echo came back
  }
  mbTxHex = hexStr(req, len);
  mbRxHex = hexStr(rx, n);

  MbErr e;
  if (n == 0) e = MB_TIMEOUT;
  else if (n < 4 || crc16(rx, n - 2) != (uint16_t)(rx[n - 2] | (rx[n - 1] << 8))) e = MB_CRC;
  else if (rx[0] != req[0]) e = MB_BAD;
  else if (rx[1] == (req[1] | 0x80) && n == 5) { e = MB_EXCEPTION; mbEx = rx[2]; }
  else if (rx[1] == req[1] && n == expectLen) e = MB_OK;
  else e = MB_BAD;

  if (e == MB_OK) mbOkCnt++;
  else {
    mbFailCnt++;
    mbLastErr = (e == MB_EXCEPTION) ? String("exception ") + mbEx : String(MB_ERR_NAME[e]);
    mbLastErrMs = millis();
  }
  return e;
}

MbErr mbRead(uint8_t fc, uint16_t reg, uint16_t qty, uint16_t *out) {
  uint8_t req[10] = {cfg.addr, fc, (uint8_t)(reg >> 8), (uint8_t)reg, (uint8_t)(qty >> 8), (uint8_t)qty};
  uint8_t rx[300]; int n;
  MbErr e = mbTransact(req, 6, rx, sizeof rx, 5 + 2 * qty, n);
  if (e == MB_OK) {
    if (rx[2] != 2 * qty) return MB_BAD;
    for (int i = 0; i < qty; i++) out[i] = (rx[3 + 2 * i] << 8) | rx[4 + 2 * i];
  }
  return e;
}

MbErr mbWriteSingle(uint16_t reg, uint16_t val) {      // FC06
  uint8_t req[10] = {cfg.addr, 6, (uint8_t)(reg >> 8), (uint8_t)reg, (uint8_t)(val >> 8), (uint8_t)val};
  uint8_t rx[64]; int n;
  MbErr e = mbTransact(req, 6, rx, sizeof rx, 8, n);
  if (e == MB_OK && memcmp(rx + 2, req + 2, 4) != 0) return MB_BAD;
  return e;
}

MbErr mbWriteMulti(uint16_t reg, uint16_t qty, const uint16_t *vals) {   // FC16
  if (qty < 1 || qty > 30) return MB_BAD;
  uint8_t req[80] = {cfg.addr, 16, (uint8_t)(reg >> 8), (uint8_t)reg, (uint8_t)(qty >> 8), (uint8_t)qty, (uint8_t)(2 * qty)};
  for (int i = 0; i < qty; i++) { req[7 + 2 * i] = vals[i] >> 8; req[8 + 2 * i] = vals[i] & 0xFF; }
  uint8_t rx[64]; int n;
  return mbTransact(req, 7 + 2 * qty, rx, sizeof rx, 8, n);
}

MbErr mbWrite(uint16_t reg, uint16_t val) {
  return cfg.writeFc == 16 ? mbWriteMulti(reg, 1, &val) : mbWriteSingle(reg, val);
}

// ============================================================ unit state
float tempOf(int r) { return regOk[r] ? (float)((int16_t)regs[r] - TEMP_OFS) : NAN; }
bool unitOn() { return regOk[R_POWER] && regs[R_POWER] == 1; }
int supLvl() { return regOk[R_SUP] ? levelOf(regs[R_SUP]) : -1; }
int exhLvl() { return regOk[R_EXH] ? levelOf(regs[R_EXH]) : -1; }

void setDesired(int on, int s, int e) {
  if (on >= 0) des.on = on;
  if (s >= 0) des.sup = s;
  if (e >= 0) des.exh = e;
  des.valid = 1; desDirty = true; desDirtyMs = millis();
}

bool writeReg(uint16_t reg, uint16_t val) {
  MbErr e = mbWrite(reg, val);
  ownWriteMs = millis();
  if (e == MB_OK) { regs[reg] = val; regOk[reg] = true; pollNow = true; return true; }
  addLog("Write R%u = %u failed: %s", reg, val, mbLastErr.c_str());
  return false;
}

bool setPower(bool on, const char *src) {
  bool ok = writeReg(R_POWER, on ? 1 : 0);
  if (ok) {
    setDesired(on, -1, -1);
    if (on) activeRule = RULE_UNSET;
    addLog("%s: unit %s", src, on ? "ON" : "OFF");
  }
  return ok;
}

bool setSpeeds(int s, int e, const char *src) {
  s = constrain(s, 0, 10); e = constrain(e, 0, 10);
  bool ok;
  if (cfg.writeFc == 16) {
    uint16_t v[2] = {SPEED_CODE[s], SPEED_CODE[e]};
    ok = mbWriteMulti(R_SUP, 2, v) == MB_OK;
    ownWriteMs = millis();
    if (ok) { regs[R_SUP] = v[0]; regs[R_EXH] = v[1]; regOk[R_SUP] = regOk[R_EXH] = true; pollNow = true; }
    else addLog("Write R10/R11 failed: %s", mbLastErr.c_str());
  } else {
    ok = writeReg(R_SUP, SPEED_CODE[s]) && writeReg(R_EXH, SPEED_CODE[e]);
  }
  if (ok) { setDesired(-1, s, e); addLog("%s: speed S%d / E%d", src, s, e); }
  return ok;
}

// Changes made on the wall controller become the new stored state
void adoptState() {
  if (restorePending || millis() - ownWriteMs < 8000) return;
  if (!regOk[R_POWER] || !regOk[R_SUP] || !regOk[R_EXH]) return;
  int on = regs[R_POWER] == 1, s = levelOf(regs[R_SUP]), e = levelOf(regs[R_EXH]);
  if (!des.valid) { setDesired(on, on && s >= 0 ? s : 5, on && e >= 0 ? e : 5); return; }
  bool changed = on != des.on;
  if (on && s >= 0 && e >= 0 && (s != des.sup || e != des.exh)) changed = true;
  if (!changed) return;
  if (on && s >= 0 && e >= 0) setDesired(on, s, e); else setDesired(on, -1, -1);
  manualMs = millis();
  addLog("Changed on the unit: %s S%d / E%d", on ? "ON" : "OFF", s, e);
}

void restoreTick() {
  if (!restorePending || (int32_t)(millis() - restoreAtMs) < 0) return;
  restorePending = false;
  if (!cfg.autoRestart || !des.valid || !linkOk) return;
  bool on = unitOn();
  if (des.on) {
    if (!on || supLvl() != des.sup || exhLvl() != des.exh) {
      addLog("Auto-restart: restoring ON S%d / E%d", des.sup, des.exh);
      if (!on) setPower(true, "Auto-restart");
      setSpeeds(des.sup, des.exh, "Auto-restart");
      lastRestore = nowT();
    }
  } else if (on) {
    addLog("Auto-restart: restoring OFF");
    setPower(false, "Auto-restart");
    lastRestore = nowT();
  }
}

// ============================================================ temperature rules
bool ruleMatch(const Rule &r, float out, float room, float h) {
  if (!isnan(r.outMin) && out < r.outMin - h) return false;
  if (!isnan(r.outMax) && out >= r.outMax + h) return false;
  if (!isnan(r.roomMin) && room < r.roomMin - h) return false;
  if (!isnan(r.roomMax) && room >= r.roomMax + h) return false;
  if (r.cmp == 1 && !(out <= room - (r.d - h))) return false;
  if (r.cmp == 2 && !(out > room - h)) return false;
  return true;
}

void evalRules() {
  if (!unitOn()) { activeRule = RULE_UNSET; return; }
  float out = tempOf(R_OA), room = tempOf(R_RA);
  if (isnan(out) || isnan(room) || (regOk[R_ERR] && (regs[R_ERR] & (ERR_OUT | ERR_ROOM)))) return;
  int cand = -1;
  for (int i = 0; i < cfg.nRules; i++)
    if (cfg.rules[i].en && ruleMatch(cfg.rules[i], out, room, 0)) { cand = i; break; }
  // keep the active rule while it still matches with hysteresis, unless a higher-priority rule matches
  if (activeRule >= 0 && activeRule < cfg.nRules && cfg.rules[activeRule].en &&
      ruleMatch(cfg.rules[activeRule], out, room, cfg.hyst) && (cand < 0 || cand >= activeRule))
    cand = activeRule;
  if (cand == activeRule) return;
  if (activeRule != RULE_UNSET && millis() - ruleSwitchMs < cfg.holdMin * 60000UL) return;
  activeRule = cand;
  ruleSwitchMs = millis();
  char src[24];
  if (cand >= 0) {
    snprintf(src, sizeof src, "Rule %d", cand + 1);
    if (supLvl() != cfg.rules[cand].sup || exhLvl() != cfg.rules[cand].exh) setSpeeds(cfg.rules[cand].sup, cfg.rules[cand].exh, src);
  } else if (cfg.fallback == 1) {
    if (supLvl() != 5 || exhLvl() != 5) setSpeeds(5, 5, "No rule matched");
  }
}

// ============================================================ weekly timer
// Turns the unit on with the fan speeds last set on the Dashboard (stored state), or off.
bool timerSwitch(bool on, const char *src) {
  bool ok = true;
  if (on) {
    if (!unitOn()) ok = setPower(true, src);
    if (ok && !cfg.rulesOn && des.valid && (supLvl() != des.sup || exhLvl() != des.exh))
      ok = setSpeeds(des.sup, des.exh, src);
  } else if (unitOn() || !regOk[R_POWER]) {
    ok = setPower(false, src);
  }
  time_t t = time(nullptr); struct tm tm; localtime_r(&t, &tm);
  char b[80];
  snprintf(b, sizeof b, "%02d:%02d %s %s%s", tm.tm_hour, tm.tm_min, src, on ? "ON" : "OFF",
           ok ? "" : (" failed: " + mbLastErr).c_str());
  timerLast = b;
  if (!ok) addLog("%s", b);
  return ok;
}

// Applies the state the schedule wants right now (used when the timer is saved or the time syncs)
void applyTimerNow(const char *src) {
  if (!cfg.timerOn || !timeValid()) return;
  time_t t = time(nullptr); struct tm tm; localtime_r(&t, &tm);
  int mod = tm.tm_hour * 60 + tm.tm_min, day = (tm.tm_wday + 6) % 7;
  bool want = false;
  for (int k = 0; k < 2; k++) {
    const Period &p = cfg.tmr[day][k];
    if (p.en && p.on <= mod && mod < p.off) want = true;
  }
  timerSwitch(want, src);
  manualMs = 0;
}

void timerTick() {
  if (!cfg.timerOn || !timeValid()) return;
  time_t t = time(nullptr); struct tm tm; localtime_r(&t, &tm);
  int mod = tm.tm_hour * 60 + tm.tm_min, day = (tm.tm_wday + 6) % 7, key = day * 1440 + mod;
  if (key != lastTimerKey) {
    lastTimerKey = key;
    for (int k = 0; k < 2; k++) {
      const Period &p = cfg.tmr[day][k];
      if (!p.en) continue;
      if (p.on == mod) { timerSwitch(true, "Timer"); manualMs = 0; }
      if (p.off == mod) { timerSwitch(false, "Timer"); manualMs = 0; }
    }
  }
  if (cfg.revertMin && manualMs && millis() - manualMs >= cfg.revertMin * 60000UL) {
    manualMs = 0;
    applyTimerNow("Timer revert");
  }
}

// ============================================================ history storage
void pathMin(char *b, uint32_t t)  { struct tm tm; localTm(t, tm); sprintf(b, "/m/%04d%02d%02d.bin", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday); }
void pathHour(char *b, uint32_t t) { struct tm tm; localTm(t, tm); sprintf(b, "/h/%04d%02d.bin", tm.tm_year + 1900, tm.tm_mon + 1); }
void pathDay(char *b, uint32_t t)  { struct tm tm; localTm(t, tm); sprintf(b, "/d/%04d.bin", tm.tm_year + 1900); }

Sample fromMin(const MinRec &r) {
  return {r.t, fromI16(r.room), fromI16(r.out), fromI16(r.exh), fromI16(r.sup), 1, r.kept / 10.0f, r.lost / 10.0f, r.fan / 100.0f};
}
Sample fromHour(const HourRec &r) {
  return {r.t, fromI16(r.room), fromI16(r.out), fromI16(r.exh), fromI16(r.sup), (float)r.n, (float)r.kept, (float)r.lost, r.fan / 10.0f};
}
Sample fromDay(const DayRec &r) {
  return {r.t, fromI16(r.room), fromI16(r.out), fromI16(r.exh), fromI16(r.sup), (float)r.n, (float)r.kept, (float)r.lost, r.fan / 10.0f};
}

// Reads fixed-size records (first field uint32 t, sorted by t) with t0 <= t < t1
void readRecs(const char *path, size_t sz, uint32_t t0, uint32_t t1, const RecCb &cb) {
  if (!LittleFS.exists(path)) return;
  File f = LittleFS.open(path, "r");
  if (!f) return;
  size_t cnt = f.size() / sz, lo = 0, hi = cnt;
  uint8_t one[32];
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    f.seek(mid * sz);
    f.read(one, sz);
    uint32_t t; memcpy(&t, one, 4);
    if (t < t0) lo = mid + 1; else hi = mid;
  }
  f.seek(lo * sz);
  uint8_t buf[32 * 28];
  size_t per = sizeof buf / sz;
  while (true) {
    size_t got = f.read(buf, per * sz) / sz;
    if (!got) break;
    for (size_t i = 0; i < got; i++) {
      uint32_t t; memcpy(&t, buf + i * sz, 4);
      if (t >= t1) { f.close(); return; }
      if (t >= t0) cb(buf + i * sz);
    }
  }
  f.close();
}

void eachMin(uint32_t t0, uint32_t t1, const SampleCb &cb) {
  if (fsOk) {
    char p[24];
    for (uint32_t d = dayStartOf(t0); d < t1; d = nextDay(d)) {
      pathMin(p, d);
      readRecs(p, sizeof(MinRec), t0, t1, [&](const uint8_t *b) { MinRec r; memcpy(&r, b, sizeof r); cb(fromMin(r)); });
    }
  }
  for (int i = 0; i < minBufN; i++)
    if (minBuf[i].t >= t0 && minBuf[i].t < t1) cb(fromMin(minBuf[i]));
}

void eachHourFiles(uint32_t t0, uint32_t t1, const SampleCb &cb) {
  if (!fsOk) return;
  char p[24];
  for (uint32_t m = monthStartOf(t0); m < t1; m = nextMonth(m)) {
    pathHour(p, m);
    readRecs(p, sizeof(HourRec), t0, t1, [&](const uint8_t *b) { HourRec r; memcpy(&r, b, sizeof r); cb(fromHour(r)); });
  }
}

// Hours from files, plus the hours not yet rolled up (built from minute records)
void eachHour(uint32_t t0, uint32_t t1, const SampleCb &cb) {
  eachHourFiles(t0, t1, cb);
  uint32_t now = nowT(), curH = now - now % 3600;
  uint32_t h = lastHourDone ? lastHourDone + 3600 : curH;
  if (h < curH - 2 * 3600) h = curH - 2 * 3600;
  for (; h <= curH; h += 3600) {
    if (h < t0 || h >= t1) continue;
    Agg a;
    eachMin(h, h + 3600, [&](const Sample &s) { a.add(s); });
    if (a.w > 0) cb(a.get(h));
  }
}

// Days from files, plus today (built from hours)
void eachDay(uint32_t t0, uint32_t t1, const SampleCb &cb) {
  if (fsOk) {
    char p[24];
    for (uint32_t y = yearStartOf(t0); y < t1; y = nextYear(y)) {
      pathDay(p, y);
      readRecs(p, sizeof(DayRec), t0, t1, [&](const uint8_t *b) { DayRec r; memcpy(&r, b, sizeof r); cb(fromDay(r)); });
    }
  }
  uint32_t today = dayStartOf(nowT());
  uint32_t d = lastDayDone ? nextDay(lastDayDone) : today;
  for (; d <= today; d = nextDay(d)) {
    if (d < t0 || d >= t1) continue;
    Agg a;
    eachHour(d, nextDay(d), [&](const Sample &s) { a.add(s); });
    if (a.w > 0) cb(a.get(d));
  }
}

void flushMin() {
  if (!fsOk) { minBufN = 0; return; }
  char p[24], q[24];
  int i = 0;
  while (i < minBufN) {
    pathMin(p, minBuf[i].t);
    File f = LittleFS.open(p, "a");
    int j = i;
    while (j < minBufN) {
      pathMin(q, minBuf[j].t);
      if (strcmp(p, q)) break;
      if (f) f.write((const uint8_t *)&minBuf[j], sizeof(MinRec));
      j++;
    }
    if (f) f.close(); else addLog("Cannot write %s", p);
    i = j;
  }
  minBufN = 0;
}

void appendRec(const char *path, const void *r, size_t sz) {
  File f = LittleFS.open(path, "a");
  if (!f) { addLog("Cannot write %s", path); return; }
  f.write((const uint8_t *)r, sz);
  f.close();
}

uint32_t lastRecT(const char *dir, size_t sz) {
  File root = LittleFS.open(dir);
  if (!root || !root.isDirectory()) return 0;
  String best;
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    String nm = f.name();
    if (nm > best) best = nm;
  }
  if (!best.length()) return 0;
  String p = String(dir) + "/" + best;
  File f = LittleFS.open(p, "r");
  if (!f || f.size() < sz) return 0;
  f.seek((f.size() / sz - 1) * sz);
  uint32_t t = 0; f.read((uint8_t *)&t, 4); f.close();
  return t;
}

void deleteOlder(const char *dir, long keepFrom, bool keepNewest) {
  File root = LittleFS.open(dir);
  if (!root || !root.isDirectory()) return;
  std::vector<String> names;
  for (File f = root.openNextFile(); f; f = root.openNextFile()) names.push_back(f.name());
  std::sort(names.begin(), names.end());
  for (size_t i = 0; i < names.size(); i++) {
    if (keepNewest && i + 1 == names.size()) break;
    if (keepFrom >= 0 && names[i].toInt() >= keepFrom) continue;
    LittleFS.remove(String(dir) + "/" + names[i]);
    addLog("History: removed %s/%s", dir, names[i].c_str());
    if (keepFrom < 0) return;     // low-space mode removes one file per call
  }
}

void cleanupHistory() {
  struct tm tm;
  uint32_t now = nowT();
  localTm(now - 7 * 86400UL, tm);
  deleteOlder("/m", (tm.tm_year + 1900) * 10000L + (tm.tm_mon + 1) * 100 + tm.tm_mday, false);
  localTm(now - 730 * 86400UL, tm);
  deleteOlder("/h", (tm.tm_year + 1900) * 100L + tm.tm_mon + 1, false);
  localTm(now, tm);
  deleteOlder("/d", tm.tm_year + 1900 - 10, false);
  for (int i = 0; i < 20 && LittleFS.totalBytes() - LittleFS.usedBytes() < 80 * 1024; i++) {
    File r = LittleFS.open("/m"); int nm = 0;
    if (r) for (File f = r.openNextFile(); f; f = r.openNextFile()) nm++;
    if (nm > 1) deleteOlder("/m", -1, true); else deleteOlder("/h", -1, true);
  }
}

void rollup() {
  if (!fsOk || !timeValid()) return;
  uint32_t now = nowT(), curH = now - now % 3600, today = dayStartOf(now);
  if (!histInit) {
    histInit = true;
    lastHourDone = lastRecT("/h", sizeof(HourRec));
    lastDayDone = lastRecT("/d", sizeof(DayRec));
  }
  char p[24];
  uint32_t h = lastHourDone ? lastHourDone + 3600 : curH - 8 * 86400UL;
  if (h + 8 * 86400UL < curH) h = curH - 8 * 86400UL;
  for (int guard = 0; h < curH && guard < 200; h += 3600, guard++) {
    Agg a;
    eachMin(h, h + 3600, [&](const Sample &s) { a.add(s); });
    if (a.w > 0) {
      Sample s = a.get(h);
      HourRec r = {h, toI16(s.room), toI16(s.out), toI16(s.exh), toI16(s.sup),
                   (uint16_t)min(65535L, lround(s.kept)), (uint16_t)min(65535L, lround(s.lost)),
                   (uint16_t)min(65535L, lround(s.fan * 10)), (uint16_t)lround(s.w)};
      pathHour(p, h);
      appendRec(p, &r, sizeof r);
    }
    lastHourDone = h;
  }
  uint32_t d = lastDayDone ? nextDay(lastDayDone) : dayStartOf(now - 8 * 86400UL);
  if (d + 40 * 86400UL < today) d = dayStartOf(now - 40 * 86400UL);
  bool rolled = false;
  for (int guard = 0; d < today && guard < 45; d = nextDay(d), guard++) {
    Agg a;
    eachHourFiles(d, nextDay(d), [&](const Sample &s) { a.add(s); });
    if (a.w > 0) {
      Sample s = a.get(d);
      DayRec r = {d, toI16(s.room), toI16(s.out), toI16(s.exh), toI16(s.sup),
                  (uint32_t)lround(s.kept), (uint32_t)lround(s.lost), (uint32_t)lround(s.fan * 10), (uint16_t)lround(s.w)};
      pathDay(p, d);
      appendRec(p, &r, sizeof r);
    }
    lastDayDone = d;
    rolled = true;
  }
  if (rolled) cleanupHistory();
}

void finishMinute() {
  if (macc.n == 0) return;
  MinRec r;
  r.t = macc.t;
  r.room = toI16(macc.room / macc.n);
  r.out = toI16(macc.out / macc.n);
  r.exh = macc.exhN ? toI16(macc.exh / macc.exhN) : NOVAL;
  r.sup = toI16(macc.sup / macc.n);
  r.kept = (uint16_t)min(65535L, lround(macc.kept * 10));
  r.lost = (uint16_t)min(65535L, lround(macc.lost * 10));
  r.fan = (uint16_t)min(65535L, lround(macc.fan * 100));
  r.flags = macc.flags; r.spd = macc.spd;
  if (minBufN >= 16) flushMin();
  minBuf[minBufN++] = r;
  memset(&macc, 0, sizeof macc);
  if (minBufN >= 10) flushMin();
}

void clearHistory() {
  minBufN = 0;
  memset(&macc, 0, sizeof macc);
  const char *dirs[] = {"/m", "/h", "/d"};
  for (const char *dir : dirs) {
    File root = LittleFS.open(dir);
    std::vector<String> names;
    if (root && root.isDirectory())
      for (File f = root.openNextFile(); f; f = root.openNextFile()) names.push_back(f.name());
    for (auto &nm : names) LittleFS.remove(String(dir) + "/" + nm);
  }
  lastHourDone = lastDayDone = 0;
  histInit = true;
  addLog("History cleared by user");
}

// ============================================================ energy
void energySample() {
  uint32_t ms = millis();
  float dt = lastEnergyMs ? (ms - lastEnergyMs) / 1000.0f : 0;
  lastEnergyMs = ms;
  dt = min(dt, cfg.pollSec * 3.0f);

  float ra = tempOf(R_RA), oa = tempOf(R_OA), sa = tempOf(R_SA), fr = tempOf(R_FR);
  bool on = unitOn();
  int ls = on ? max(0, supLvl()) : 0, le = on ? max(0, exhLvl()) : 0;
  float qs = cfg.flow[ls];
  uint16_t st = regOk[R_STATUS] ? regs[R_STATUS] : 0;
  bool defrost = st & ST_DEFROST, bypass = st & ST_BYPASS_ON;
  bool sensorsOk = !isnan(ra) && !isnan(oa) && !isnan(sa) &&
                   !(regOk[R_ERR] && (regs[R_ERR] & (ERR_OUT | ERR_ROOM)));

  // supply side: the outside air is warmed from OA to SA by the exchanger (kept); heating has to cover SA -> RA (lost)
  live.heating = on && sensorsOk && !defrost && !bypass && oa < ra && qs > 0;
  live.kept = live.heating ? max(0.0f, 0.335f * qs * (sa - oa)) : 0;
  live.lost = live.heating ? max(0.0f, 0.335f * qs * (ra - sa)) : 0;
  live.fan = on ? (cfg.fanW[ls] + cfg.fanW[le]) / 2.0f : 0;

  if (!timeValid() || !sensorsOk) return;
  uint32_t now = nowT(), key = now - now % 60;
  if (macc.n && macc.t != key) finishMinute();
  macc.t = key;
  macc.room += ra; macc.out += oa; macc.sup += sa; macc.n++;
  if (!isnan(fr)) { macc.exh += fr; macc.exhN++; }
  macc.kept += live.kept * dt / 3600; macc.lost += live.lost * dt / 3600; macc.fan += live.fan * dt / 3600;
  macc.flags = (on ? 1 : 0) | (defrost ? 2 : 0) | (bypass ? 4 : 0) | (live.heating ? 8 : 0);
  macc.spd = (ls << 4) | le;
}

void minuteTick() {
  static uint32_t lastKey = 0;
  if (!timeValid()) return;
  if (!timeWasValid) { timeWasValid = true; addLog("Time synced (NTP)"); applyTimerNow("Timer (time synced)"); }
  uint32_t now = nowT(), key = now - now % 60;
  if (key == lastKey) return;
  lastKey = key;
  if (macc.n && macc.t < key) finishMinute();
  rollup();
  timerTick();
}

// ============================================================ polling
void onPoll(bool ok) {
  if (ok) {
    failStreak = 0; lastOkMs = millis();
    if (!linkOk) { linkOk = true; addLog("Link up: addr %u, %lu %s", cfg.addr, (unsigned long)cfg.baud, FMT_NAME[cfg.fmt]); }
    if (hadFailure) { hadFailure = false; restorePending = true; restoreAtMs = millis() + cfg.restoreDelay * 1000UL; }
    energySample();
    adoptState();
    if (!restorePending && cfg.rulesOn) evalRules();
  } else {
    failStreak++;
    hadFailure = true;
    if (linkOk && failStreak >= 3) { linkOk = false; addLog("Link lost: %s", mbLastErr.c_str()); }
    if (failStreak >= 3) { for (int i = 0; i < NREG; i++) regOk[i] = false; lastSlowMs = 0; }
  }
}

// Single-register read with skip logic for registers that keep failing
void readSingle(int r) {
  if (regSkipUntil[r] && (int32_t)(millis() - regSkipUntil[r]) < 0) return;   // known-bad register, retry later
  uint16_t v;
  if (mbRead(3, r, 1, &v) == MB_OK) { regs[r] = v; regOk[r] = true; regFails[r] = 0; regSkipUntil[r] = 0; return; }
  regOk[r] = false;
  if (++regFails[r] >= 3) {
    regSkipUntil[r] = millis() + 60000;
    if (regFails[r] == 3) addLog("R%d does not answer (%s), retrying every 60 s", r, mbLastErr.c_str());
  }
}

void pollUnit() {
  lastPollMs = millis();
  uint16_t buf[NREG];
  bool ok = false;
  bool slowDue = !lastSlowMs || millis() - lastSlowMs >= 30000;   // settings registers every 30 s
  if (slowDue) lastSlowMs = millis();

  if (!singleMode) {
    // The unit answers at most 20 registers per request: R2..R21 in one read, R22 separately
    const int q = REG_BLOCK_LAST - REG_FIRST + 1;
    MbErr e = mbRead(3, REG_FIRST, q, buf);
    if (e == MB_OK) {
      for (int i = 0; i < q; i++) { regs[REG_FIRST + i] = buf[i]; regOk[REG_FIRST + i] = true; }
      ok = true;
      if (slowDue) for (int r = REG_BLOCK_LAST + 1; r <= REG_LAST; r++) readSingle(r);
    } else {
      // If a single read of the room temperature works, the link is fine and only block reads fail
      String blockErr = mbLastErr;
      if (mbRead(3, R_RA, 1, buf) == MB_OK) {
        singleMode = true;
        slowDue = true;
        addLog("Block read R%d-R%d failed (%s), switching to single-register reads", REG_FIRST, REG_BLOCK_LAST, blockErr.c_str());
      }
    }
  }
  if (singleMode) {
    // R12 is the link probe: if it does not answer, the unit is not reachable
    if (mbRead(3, R_RA, 1, buf) == MB_OK) {
      regs[R_RA] = buf[0]; regOk[R_RA] = true; ok = true;
      // live registers (R9..R20) every poll, settings (R2..R8, R21, R22) every 30 s
      for (int r = REG_FIRST; r <= REG_LAST; r++) {
        if (r == R_RA) continue;
        bool slow = r <= R_ADDR || r >= R_MODEL;
        if (slow && !slowDue) continue;
        readSingle(r);
      }
    }
  }
  // R0/R1 sit outside the R2..R21 block; read them on their own with the settings registers
  if (ok && slowDue) for (int r = R_RELOAD; r < REG_FIRST; r++) readSingle(r);
  onPoll(ok);
}

// ============================================================ web: helpers
void sendJson(const String &j) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}
void sendOk() { sendJson("{\"ok\":true}"); }
void sendErr(const String &e) { sendJson("{\"ok\":false,\"err\":\"" + jsonEsc(e) + "\"}"); }

// ============================================================ web: state
void handleState() {
  String j; j.reserve(1500);
  j += "{\"t\":"; j += timeValid() ? nowT() : 0;
  j += ",\"tv\":"; j += timeValid() ? "true" : "false";
  j += ",\"up\":"; j += millis() / 1000;
  j += ",\"link\":{\"ok\":"; j += linkOk ? "true" : "false";
  j += ",\"ago\":"; j += lastOkMs ? (long)((millis() - lastOkMs) / 1000) : -1L;
  j += ",\"okc\":"; j += mbOkCnt; j += ",\"errc\":"; j += mbFailCnt;
  j += ",\"err\":\""; j += jsonEsc(mbLastErr); j += "\",\"errAgo\":"; j += mbLastErrMs ? (long)((millis() - mbLastErrMs) / 1000) : -1L;
  j += ",\"echo\":"; j += echoSeen ? "true" : "false";
  j += ",\"single\":"; j += singleMode ? "true" : "false";
  j += ",\"addr\":"; j += cfg.addr; j += ",\"baud\":"; j += cfg.baud; j += ",\"fmt\":\""; j += FMT_NAME[cfg.fmt]; j += "\"}";
  j += ",\"r\":[";
  for (int i = 0; i < NREG; i++) { if (i) j += ','; if (regOk[i]) j += regs[i]; else j += "null"; }
  j += "],\"live\":{\"kept\":"; jNum(j, live.kept, 0);
  j += ",\"lost\":"; jNum(j, live.lost, 0);
  j += ",\"fan\":"; jNum(j, live.fan, 0);
  j += ",\"heating\":"; j += live.heating ? "true" : "false";
  j += "},\"des\":{\"valid\":"; j += des.valid; j += ",\"on\":"; j += des.on; j += ",\"s\":"; j += des.sup; j += ",\"e\":"; j += des.exh;
  j += "},\"rst\":{\"last\":"; j += lastRestore;
  j += ",\"pend\":"; j += restorePending ? max(0L, (long)(restoreAtMs - millis()) / 1000) : -1L;
  j += "},\"rule\":{\"on\":"; j += cfg.rulesOn; j += ",\"act\":"; j += activeRule;
  j += "},\"tmr\":{\"on\":"; j += cfg.timerOn; j += ",\"manual\":"; j += manualMs ? (long)((millis() - manualMs) / 1000) : -1L;
  j += ",\"last\":\""; j += jsonEsc(timerLast); j += '"';
  j += "}}";
  sendJson(j);
}

void handleLog() {
  String j; j.reserve(6000);
  j += '[';
  for (int i = 0; i < logCount; i++) {
    int idx = (logHead - logCount + i + LOG_N) % LOG_N;
    if (i) j += ',';
    j += '"'; j += jsonEsc(logLines[idx]); j += '"';
  }
  j += ']';
  sendJson(j);
}

// ============================================================ web: control
void handlePower() {
  bool on = server.arg("on").toInt() == 1;
  if (!setPower(on, "Web")) { sendErr(mbLastErr); return; }
  if (cfg.timerOn) manualMs = millis();
  sendOk();
}

void handleSpeed() {
  int s = constrain(server.arg("s").toInt(), 0, 10), e = constrain(server.arg("e").toInt(), 0, 10);
  if (!setSpeeds(s, e, "Web")) { sendErr(mbLastErr); return; }
  if (cfg.timerOn) manualMs = millis();
  sendOk();
}

// Unit settings: /api/write?r2=19&r5=-1 ...  (registers 2..7 only)
void handleWrite() {
  const int lo[8] = {0, 0, 5, 2, 15, -9, 2, 0x28}, hi[8] = {0, 0, 30, 15, 99, 5, 20, 0xC8};
  String j = "{\"res\":[";
  bool all = true, first = true;
  for (int a = 0; a < server.args(); a++) {
    String nm = server.argName(a);
    if (nm.length() < 2 || nm[0] != 'r') continue;
    int reg = nm.substring(1).toInt(), v = server.arg(a).toInt();
    if (!first) j += ',';
    first = false;
    j += "{\"r\":"; j += reg;
    if (reg < 2 || reg > 7 || v < lo[reg] || v > hi[reg]) { j += ",\"ok\":false,\"err\":\"out of range\"}"; all = false; continue; }
    uint16_t raw = (uint16_t)(int16_t)v, back = 0;
    bool ok = writeReg(reg, raw);
    String err = ok ? "" : mbLastErr;
    if (ok) {
      if (mbRead(3, reg, 1, &back) != MB_OK) { ok = false; err = "read-back: " + mbLastErr; }
      else if (back != raw) { ok = false; err = "read-back value " + String((int16_t)back); }
    }
    if (ok) addLog("Web: R%d set to %d", reg, v);
    all &= ok;
    j += ",\"ok\":"; j += ok ? "true" : "false";
    j += ",\"err\":\""; j += jsonEsc(err); j += "\"}";
  }
  j += "],\"ok\":"; j += all ? "true" : "false"; j += '}';
  sendJson(j);
}

void handleAddr() {
  int v = server.arg("v").toInt();
  if (v < 1 || v > 16) { sendErr("address must be 1..16"); return; }
  MbErr e = mbWrite(R_ADDR, v);
  if (e == MB_EXCEPTION || e == MB_CRC) { sendErr(mbLastErr); return; }
  uint8_t old = cfg.addr;
  cfg.addr = v;
  uint16_t t;
  if (mbRead(3, R_ADDR, 1, &t) != MB_OK) {
    cfg.addr = old;
    sendErr("unit does not answer on address " + String(v) + " (" + mbLastErr + "), kept " + String(old));
    return;
  }
  saveConfig();
  addLog("Modbus address changed %u -> %d", old, v);
  sendOk();
}

// Raw access: op=r3|r4 reg cnt ; op=w6 reg val ; op=w16 reg vals=a,b,c
void handleRaw() {
  String op = server.arg("op");
  int reg = server.arg("reg").toInt();
  if (reg < 0 || reg > 65535) { sendErr("bad register"); return; }
  MbErr e = MB_BAD;
  String vals;
  if (op == "r3" || op == "r4") {
    int cnt = constrain(server.arg("cnt").toInt(), 1, 30);
    uint16_t buf[30];
    e = mbRead(op == "r3" ? 3 : 4, reg, cnt, buf);
    if (e == MB_OK) for (int i = 0; i < cnt; i++) { if (i) vals += ','; vals += buf[i]; }
  } else if (op == "w6") {
    e = mbWriteSingle(reg, (uint16_t)server.arg("val").toInt());
    ownWriteMs = millis();
  } else if (op == "w16") {
    String parts[30]; uint16_t buf[30];
    int n = splitCsv(server.arg("vals"), parts, 30);
    for (int i = 0; i < n; i++) buf[i] = (uint16_t)parts[i].toInt();
    e = mbWriteMulti(reg, n, buf);
    ownWriteMs = millis();
  } else { sendErr("bad op"); return; }
  if (op[0] == 'w') { addLog("Raw %s R%d: %s", op.c_str(), reg, MB_ERR_NAME[e]); pollNow = true; }
  String j = "{\"ok\":"; j += e == MB_OK ? "true" : "false";
  j += ",\"vals\":["; j += vals; j += "],\"err\":\""; j += e == MB_OK ? "" : jsonEsc(mbLastErr);
  j += "\",\"tx\":\""; j += mbTxHex; j += "\",\"rx\":\""; j += mbRxHex; j += "\"}";
  sendJson(j);
}

// ============================================================ web: config
void handleConfigGet() {
  String j; j.reserve(2500);
  j += "{\"link\":{\"addr\":"; j += cfg.addr; j += ",\"baud\":"; j += cfg.baud; j += ",\"fmt\":"; j += cfg.fmt;
  j += ",\"poll\":"; j += cfg.pollSec; j += ",\"to\":"; j += cfg.timeoutMs;
  j += ",\"wfc\":"; j += cfg.writeFc;
  j += "},\"rst\":{\"on\":"; j += cfg.autoRestart; j += ",\"delay\":"; j += cfg.restoreDelay;
  j += "},\"rules\":{\"on\":"; j += cfg.rulesOn; j += ",\"hyst\":"; jNum(j, cfg.hyst, 1);
  j += ",\"hold\":"; j += cfg.holdMin; j += ",\"fb\":"; j += cfg.fallback; j += ",\"list\":[";
  for (int i = 0; i < cfg.nRules; i++) {
    const Rule &r = cfg.rules[i];
    if (i) j += ',';
    j += '['; j += r.en; j += ','; jNum(j, r.outMin, 1); j += ','; jNum(j, r.outMax, 1); j += ',';
    jNum(j, r.roomMin, 1); j += ','; jNum(j, r.roomMax, 1); j += ','; j += r.cmp; j += ','; jNum(j, r.d, 1);
    j += ','; j += r.sup; j += ','; j += r.exh; j += ']';
  }
  j += "]},\"tmr\":{\"on\":"; j += cfg.timerOn; j += ",\"rev\":"; j += cfg.revertMin; j += ",\"d\":[";
  for (int d = 0; d < 7; d++) {
    if (d) j += ',';
    j += '[';
    for (int k = 0; k < 2; k++) {
      const Period &p = cfg.tmr[d][k];
      if (k) j += ',';
      j += '['; j += p.en; j += ','; j += p.on; j += ','; j += p.off; j += ']';
    }
    j += ']';
  }
  j += "]},\"en\":{\"hp\":"; jNum(j, cfg.heatPrice, 2); j += ",\"ep\":"; jNum(j, cfg.elPrice, 2);
  j += ",\"flow\":[";
  for (int i = 0; i < 11; i++) { if (i) j += ','; j += cfg.flow[i]; }
  j += "],\"fanw\":[";
  for (int i = 0; i < 11; i++) { if (i) j += ','; j += cfg.fanW[i]; }
  j += "]}}";
  sendJson(j);
}

void handleConfigSet() {
  String sec = server.arg("section");
  if (sec == "link") {
    const uint32_t okBaud[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
    uint32_t b = server.arg("baud").toInt();
    bool good = false;
    for (uint32_t x : okBaud) if (x == b) good = true;
    if (!good) { sendErr("bad baud"); return; }
    cfg.addr = constrain(server.arg("addr").toInt(), 1, 247);
    cfg.baud = b;
    cfg.fmt = constrain(server.arg("fmt").toInt(), 0, 3);
    cfg.pollSec = constrain(server.arg("poll").toInt(), 1, 60);
    cfg.timeoutMs = constrain(server.arg("to").toInt(), 50, 2000);
    cfg.writeFc = server.arg("wfc").toInt() == 16 ? 16 : 6;
    uartApply();
    singleMode = false; echoSeen = false; pollNow = true;
    memset(regFails, 0, sizeof regFails); memset(regSkipUntil, 0, sizeof regSkipUntil);
    addLog("Link settings: addr %u, %lu %s, write FC%02u", cfg.addr, (unsigned long)cfg.baud, FMT_NAME[cfg.fmt], cfg.writeFc);
  } else if (sec == "rst") {
    cfg.autoRestart = server.arg("on").toInt() ? 1 : 0;
    cfg.restoreDelay = constrain(server.arg("delay").toInt(), 0, 600);
  } else if (sec == "rules") {
    int n = constrain(server.arg("n").toInt(), 0, MAX_RULES);
    for (int i = 0; i < n; i++) {
      String f[9];
      if (splitCsv(server.arg("r" + String(i)), f, 9) != 9) { sendErr("rule " + String(i + 1) + ": bad format"); return; }
      Rule r;
      r.en = f[0].toInt() ? 1 : 0;
      r.outMin = parseF(f[1]); r.outMax = parseF(f[2]); r.roomMin = parseF(f[3]); r.roomMax = parseF(f[4]);
      r.cmp = constrain(f[5].toInt(), 0, 2);
      r.d = isnan(parseF(f[6])) ? 0 : parseF(f[6]);
      r.sup = constrain(f[7].toInt(), 0, 10); r.exh = constrain(f[8].toInt(), 0, 10);
      cfg.rules[i] = r;
    }
    cfg.nRules = n;
    cfg.rulesOn = server.arg("on").toInt() ? 1 : 0;
    cfg.hyst = constrain(parseF(server.arg("hyst")), 0.0f, 5.0f);
    if (isnan(cfg.hyst)) cfg.hyst = 1;
    cfg.holdMin = constrain(server.arg("hold").toInt(), 0, 240);
    cfg.fallback = server.arg("fb").toInt() == 1 ? 1 : 0;
    activeRule = RULE_UNSET;
    addLog("Temperature rules saved (%d rules, %s)", n, cfg.rulesOn ? "enabled" : "disabled");
  } else if (sec == "tmr") {
    Period t[7][2];
    for (int d = 0; d < 7; d++) {
      String f[6];   // en,on,off,en,on,off
      if (splitCsv(server.arg("d" + String(d)), f, 6) != 6) { sendErr("day " + String(d + 1) + ": bad format"); return; }
      for (int k = 0; k < 2; k++) {
        Period p = {(uint8_t)(f[3 * k].toInt() ? 1 : 0), (uint16_t)constrain(f[3 * k + 1].toInt(), 0, 1439),
                    (uint16_t)constrain(f[3 * k + 2].toInt(), 0, 1439), 0};
        if (p.en && p.off <= p.on) { sendErr("day " + String(d + 1) + " period " + String(k + 1) + ": OFF must be later than ON"); return; }
        t[d][k] = p;
      }
    }
    memcpy(cfg.tmr, t, sizeof t);
    cfg.timerOn = server.arg("on").toInt() ? 1 : 0;
    cfg.revertMin = constrain(server.arg("rev").toInt(), 0, 1440);
    lastTimerKey = -1;
    addLog("Weekly timer saved (%s)", cfg.timerOn ? "enabled" : "disabled");
    saveConfig();
    applyTimerNow("Timer (saved)");
  } else if (sec == "en") {
    float hp = parseF(server.arg("hp")), ep = parseF(server.arg("ep"));
    if (!isnan(hp)) cfg.heatPrice = hp;
    if (!isnan(ep)) cfg.elPrice = ep;
    String f[11];
    if (server.hasArg("flow") && splitCsv(server.arg("flow"), f, 11) == 11)
      for (int i = 0; i < 11; i++) cfg.flow[i] = constrain(f[i].toInt(), 0, 2000);
    if (server.hasArg("fanw") && splitCsv(server.arg("fanw"), f, 11) == 11)
      for (int i = 0; i < 11; i++) cfg.fanW[i] = constrain(f[i].toInt(), 0, 2000);
    cfg.flow[0] = cfg.fanW[0] = 0;
  } else { sendErr("unknown section"); return; }
  saveConfig();
  sendOk();
}

// ============================================================ web: history
int slotIndex(const std::vector<uint32_t> &st, uint32_t t) {
  auto it = std::upper_bound(st.begin(), st.end(), t);
  return it == st.begin() ? -1 : int(it - st.begin()) - 1;
}

std::vector<uint32_t> lastDays(int n) {
  std::vector<uint32_t> v;
  uint32_t d = dayStartOf(nowT());
  for (int i = 0; i < n; i++) { v.push_back(d); d = prevDay(d); }
  std::reverse(v.begin(), v.end());
  return v;
}

void fillSlots(const std::vector<uint32_t> &st, std::vector<Agg> &ag, int src) {
  uint32_t t0 = st.front(), t1 = nowT() + 1;
  SampleCb put = [&](const Sample &s) { int i = slotIndex(st, s.t); if (i >= 0) ag[i].add(s); };
  if (src == 0) eachMin(t0, t1, put);
  else if (src == 1) eachHour(t0, t1, put);
  else eachDay(t0, t1, put);
}

void handleHistory() {
  if (!timeValid()) { sendErr("time not synced yet"); return; }
  if (!fsOk) { sendErr("file system not available"); return; }
  String r = server.arg("range");
  uint32_t now = nowT(), curH = now - now % 3600;
  std::vector<uint32_t> ts, bs;
  int tSrc, bSrc;
  if (r == "7d") {
    for (int i = 167; i >= 0; i--) ts.push_back(curH - i * 3600);
    tSrc = 1; bs = lastDays(7); bSrc = 2;
  } else if (r == "30d") {
    ts = lastDays(30); tSrc = 2; bs = ts; bSrc = 2;
  } else if (r == "12m") {
    ts = lastDays(365); tSrc = 2;
    uint32_t m = monthStartOf(now);
    for (int i = 0; i < 12; i++) { bs.push_back(m); m = prevMonth(m); }
    std::reverse(bs.begin(), bs.end());
    bSrc = 2;
  } else {
    r = "24h";
    uint32_t m10 = now - now % 600;
    for (int i = 143; i >= 0; i--) ts.push_back(m10 - i * 600);
    tSrc = 0;
    for (int i = 23; i >= 0; i--) bs.push_back(curH - i * 3600);
    bSrc = 1;
  }
  std::vector<Agg> ta(ts.size()), ba(bs.size());
  fillSlots(ts, ta, tSrc);
  if (r == "30d") ba = ta; else fillSlots(bs, ba, bSrc);

  String j; j.reserve(ts.size() * 32 + bs.size() * 40 + 100);
  j += "{\"range\":\""; j += r; j += "\",\"now\":"; j += now; j += ",\"temp\":[";
  for (size_t i = 0; i < ts.size(); i++) {
    if (i) j += ',';
    Sample s = ta[i].get(ts[i]);
    j += '['; j += ts[i];
    float v[4] = {s.room, s.out, s.exh, s.sup};
    for (float x : v) { j += ','; if (s.w > 0 && !isnan(x)) j += lroundf(x * 10); else j += "null"; }
    j += ']';
  }
  j += "],\"bars\":[";
  for (size_t i = 0; i < bs.size(); i++) {
    if (i) j += ',';
    Sample s = ba[i].get(bs[i]);
    j += '['; j += bs[i];
    if (s.w > 0) {
      j += ','; j += lroundf(s.kept); j += ','; j += lroundf(s.lost); j += ','; jNum(j, s.fan, 1);
      j += ','; if (isnan(s.out)) j += "null"; else j += lroundf(s.out * 10);
    } else j += ",null,null,null,null";
    j += ']';
  }
  j += "]}";
  sendJson(j);
}

void handleCsv() {
  if (!timeValid() || !fsOk) { server.send(503, "text/plain", "History not available (time not synced or no file system)"); return; }
  String r = server.arg("range");
  uint32_t now = nowT(), curH = now - now % 3600, t0;
  int src;
  if (r == "7d") { t0 = curH - 167 * 3600; src = 1; }
  else if (r == "30d") { t0 = lastDays(30).front(); src = 2; }
  else if (r == "12m") { t0 = lastDays(365).front(); src = 2; }
  else { r = "24h"; t0 = now - 86400; src = 0; }
  server.sendHeader("Content-Disposition", "attachment; filename=hrv_" + r + ".csv");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  String chunk; chunk.reserve(2600);
  chunk = "time,RA_C,OA_C,FR_C,SA_C,minutes,heat_kept_Wh,heat_lost_Wh,fan_Wh\r\n";
  SampleCb row = [&](const Sample &s) {
    struct tm tm; localTm(s.t, tm);
    char b[160];
    snprintf(b, sizeof b, "%04d-%02d-%02d %02d:%02d,", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
    chunk += b;
    float v[4] = {s.room, s.out, s.exh, s.sup};
    for (float x : v) { if (!isnan(x)) chunk += String(x, 1); chunk += ','; }
    snprintf(b, sizeof b, "%.0f,%.1f,%.1f,%.2f\r\n", s.w, s.kept, s.lost, s.fan);
    chunk += b;
    if (chunk.length() > 2000) { server.sendContent(chunk); chunk = ""; }
  };
  if (src == 0) eachMin(t0, now + 1, row);
  else if (src == 1) eachHour(t0, now + 1, row);
  else eachDay(t0, now + 1, row);
  if (chunk.length()) server.sendContent(chunk);
  server.sendContent("");
}

void dirInfo(const char *dir, size_t &bytes, int &files, String &oldest) {
  bytes = 0; files = 0; oldest = "";
  File root = LittleFS.open(dir);
  if (!root || !root.isDirectory()) return;
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    bytes += f.size(); files++;
    String nm = f.name();
    if (!oldest.length() || nm < oldest) oldest = nm;
  }
  int dot = oldest.indexOf('.');
  if (dot > 0) oldest = oldest.substring(0, dot);
}

void handleStorage() {
  if (!fsOk) { sendErr("file system not available"); return; }
  String j = "{\"ok\":true,\"total\":"; j += LittleFS.totalBytes(); j += ",\"used\":"; j += LittleFS.usedBytes();
  const char *dirs[] = {"/m", "/h", "/d"}, *keys[] = {"min", "hour", "day"};
  for (int i = 0; i < 3; i++) {
    size_t b; int n; String o;
    dirInfo(dirs[i], b, n, o);
    j += ",\""; j += keys[i]; j += "\":{\"bytes\":"; j += b; j += ",\"files\":"; j += n; j += ",\"oldest\":\""; j += o; j += "\"}";
  }
  j += '}';
  sendJson(j);
}

void handleClear() {
  if (!fsOk) { sendErr("file system not available"); return; }
  clearHistory();
  sendOk();
}

// ============================================================ web: pages
void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", PAGE_HTML);
}

void handleRelays() {
  String page;
  page.reserve(3000);
  page += F("<!doctype html><html><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>HKL-EA8 relays</title>"
            "<link rel=\"icon\" type=\"image/svg+xml\" href=\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='7' fill='%231f7a5c'/%3E%3Cg fill='%23fff'%3E%3Cpath id='b' d='M16 16C13 12 13 6 17.5 4.5C21.5 3.5 22.5 9 16 16Z'/%3E%3Cuse href='%23b' transform='rotate(90 16 16)'/%3E%3Cuse href='%23b' transform='rotate(180 16 16)'/%3E%3Cuse href='%23b' transform='rotate(270 16 16)'/%3E%3C/g%3E%3Ccircle cx='16' cy='16' r='2.6' fill='%231f7a5c' stroke='%23fff' stroke-width='1.4'/%3E%3C/svg%3E\"></head>"
            "<body style='font-family:Arial;max-width:700px;margin:25px auto;padding:10px'>"
            "<h2>HKL-EA8 Relay Control</h2>");
  for (int ch = 0; ch < 8; ch++) {
    page += "<p>Relay " + String(ch + 1) + " — <b>" + (relayState[ch] ? "ON" : "OFF") +
            "</b> &nbsp; <a href='/relay?ch=" + String(ch) + "&state=1'>ON</a> &nbsp; "
            "<a href='/relay?ch=" + String(ch) + "&state=0'>OFF</a></p>";
  }
  page += F("<hr><p><a href='/'>Back to HRV control</a></p></body></html>");
  server.send(200, "text/html", page);
}

void handleRelay() {
  int ch = server.arg("ch").toInt(), state = server.arg("state").toInt();
  if (!server.hasArg("ch") || ch < 0 || ch > 7 || (state != 0 && state != 1)) {
    server.send(400, "text/plain", "Invalid relay channel or state");
    return;
  }
  relayState[ch] = state == 1;
  relays.digitalWrite(ch, relayState[ch] ? HIGH : LOW);
  server.sendHeader("Location", "/relays");
  server.send(303, "text/plain", "Relay updated");
}

// ============================================================ setup / loop
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nHKL-EA8 HRV controller");

  loadConfig();
  if (RS485_DE >= 0) { pinMode(RS485_DE, OUTPUT); digitalWrite(RS485_DE, LOW); }
  uartApply();

  if (!relays.begin()) Serial.println("WARNING: PCF8574 relay expander did not initialize.");
  for (int ch = 0; ch < 8; ch++) { relays.pinMode(ch, OUTPUT); relays.digitalWrite(ch, LOW); }

  fsOk = LittleFS.begin(true);
  if (fsOk) {
    const char *dirs[] = {"/m", "/h", "/d"};
    for (const char *d : dirs) if (!LittleFS.exists(d)) LittleFS.mkdir(d);
  } else {
    Serial.println("WARNING: LittleFS not available, history disabled.");
  }

  if (!ETH.begin(ETH_TYPE, ETH_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLK_MODE))
    Serial.println("ETH.begin() reported failure.");
  ETH.config(local_IP, gateway, subnet, dns1, dns2);
  configTzTime(TZ_INFO, NTP1, NTP2, NTP3);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/favicon.ico", HTTP_GET, [] { server.send(204); });  // the pages carry an inline SVG icon
  server.on("/relays", HTTP_GET, handleRelays);
  server.on("/relay", HTTP_GET, handleRelay);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigSet);
  server.on("/api/power", HTTP_POST, handlePower);
  server.on("/api/speed", HTTP_POST, handleSpeed);
  server.on("/api/write", HTTP_POST, handleWrite);
  server.on("/api/addr", HTTP_POST, handleAddr);
  server.on("/api/raw", HTTP_POST, handleRaw);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.on("/api/csv", HTTP_GET, handleCsv);
  server.on("/api/storage", HTTP_GET, handleStorage);
  server.on("/api/history/clear", HTTP_POST, handleClear);
  server.begin();

  addLog("Started: http://%s/  (Modbus addr %u, %lu %s)", local_IP.toString().c_str(), cfg.addr, (unsigned long)cfg.baud, FMT_NAME[cfg.fmt]);
}

void loop() {
  server.handleClient();
  if (pollNow || millis() - lastPollMs >= cfg.pollSec * 1000UL) {
    pollNow = false;
    pollUnit();
  }
  restoreTick();
  minuteTick();
  saveDesiredIfDirty();
}
