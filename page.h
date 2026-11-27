// Web UI served at "/". Built from the approved mockup (mockup.html) and wired to the /api endpoints.
#pragma once
#include <pgmspace.h>

const char PAGE_HTML[] PROGMEM = R"HTMLPAGE(
<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HRV Control — CH-HRV3.5KDC</title>
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' rx='7' fill='%231f7a5c'/%3E%3Cg fill='%23fff'%3E%3Cpath id='b' d='M16 16C13 12 13 6 17.5 4.5C21.5 3.5 22.5 9 16 16Z'/%3E%3Cuse href='%23b' transform='rotate(90 16 16)'/%3E%3Cuse href='%23b' transform='rotate(180 16 16)'/%3E%3Cuse href='%23b' transform='rotate(270 16 16)'/%3E%3C/g%3E%3Ccircle cx='16' cy='16' r='2.6' fill='%231f7a5c' stroke='%23fff' stroke-width='1.4'/%3E%3C/svg%3E">
<style>
:root{--bg:#f4f6f8;--card:#fff;--ink:#1d2329;--mut:#6b7680;--line:#dde3e8;--acc:#1f7a5c;--acc2:#e6f3ee;
--warn:#b7791f;--warnbg:#fff6e0;--err:#c0392b;--errbg:#fdecea;--blue:#2563a8;--bluebg:#e8f0fb}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,Segoe UI,Arial,sans-serif;background:var(--bg);color:var(--ink)}
header{background:#16202a;color:#fff;padding:10px 16px;display:flex;align-items:center;gap:14px;flex-wrap:wrap}
header h1{font-size:17px;margin:0;font-weight:600}
header .sub{font-size:12px;color:#9fb0bf}
.pill{display:inline-flex;align-items:center;gap:6px;font-size:12px;padding:3px 9px;border-radius:20px;background:#24313d}
.dot{width:8px;height:8px;border-radius:50%;background:#3ad07f}
.dot.bad{background:#e74c3c}
header .sp{flex:1}
header a{color:#cfe;font-size:13px}
nav{display:flex;gap:2px;background:#fff;border-bottom:1px solid var(--line);padding:0 10px;overflow-x:auto}
nav button{border:0;background:none;padding:11px 14px;font-size:14px;cursor:pointer;color:var(--mut);border-bottom:3px solid transparent;white-space:nowrap}
nav button.on{color:var(--ink);border-bottom-color:var(--acc);font-weight:600}
main{max-width:1100px;margin:0 auto;padding:14px}
.tab{display:none}.tab.on{display:block}
.grid{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(250px,1fr))}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px}
.card h3{margin:0 0 10px;font-size:14px;text-transform:uppercase;letter-spacing:.04em;color:var(--mut);display:flex;justify-content:space-between;align-items:center}
.reg{font-size:11px;color:var(--mut);font-weight:400;text-transform:none;letter-spacing:0;background:#eef1f4;padding:1px 6px;border-radius:4px;font-family:monospace}
.big{font-size:34px;font-weight:700;line-height:1.1}
.unit{font-size:16px;color:var(--mut);font-weight:400}
.temps{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}
@media(max-width:700px){.temps{grid-template-columns:repeat(2,1fr)}}
.t{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px}
.t .lbl{font-size:12px;color:var(--mut);display:flex;justify-content:space-between}
.power{display:flex;align-items:center;gap:16px}
.pwr{width:74px;height:74px;border-radius:50%;border:0;font-size:30px;cursor:pointer;background:#d9dee3;color:#fff}
.pwr.on{background:var(--acc);box-shadow:0 0 0 6px var(--acc2)}
.speeds{display:flex;gap:4px;flex-wrap:wrap;margin:6px 0}
.speeds button{width:34px;height:34px;border:1px solid var(--line);background:#fff;border-radius:6px;cursor:pointer;font-weight:600}
.speeds button.on{background:var(--acc);color:#fff;border-color:var(--acc)}
.speeds button.stop{width:auto;padding:0 10px}
.row{display:flex;align-items:center;gap:8px;margin:6px 0;flex-wrap:wrap}
.flags{display:flex;flex-wrap:wrap;gap:6px}
.flag{font-size:12px;padding:4px 9px;border-radius:14px;background:#eef1f4;color:var(--mut)}
.flag.on{background:var(--acc2);color:var(--acc);font-weight:600}
.flag.warn{background:var(--warnbg);color:var(--warn);font-weight:600}
.flag.err{background:var(--errbg);color:var(--err);font-weight:600}
table{border-collapse:collapse;width:100%;font-size:13px}
td,th{border-bottom:1px solid var(--line);padding:7px 6px;text-align:left;vertical-align:middle}
th{font-size:12px;color:var(--mut);font-weight:600;background:#fafbfc}
input[type=number],select,input[type=text],input[type=time]{padding:5px 7px;border:1px solid #c8d0d7;border-radius:6px;font-size:14px}
input[type=number]{width:80px}
.btn{padding:6px 12px;border:1px solid #c8d0d7;background:#fff;border-radius:6px;cursor:pointer;font-size:13px}
.btn.pri{background:var(--acc);border-color:var(--acc);color:#fff}
.btn.dng{background:#fff;border-color:var(--err);color:var(--err)}
.btn:disabled{opacity:.45;cursor:default}
.mut{color:var(--mut);font-size:12px}
.changed{background:var(--warnbg)}
.note{background:var(--bluebg);border-left:4px solid var(--blue);padding:8px 12px;border-radius:4px;font-size:13px;margin:8px 0}
.note.warn{background:var(--warnbg);border-color:var(--warn)}
.switch{position:relative;display:inline-block;width:42px;height:24px}
.switch input{display:none}
.slider{position:absolute;inset:0;background:#cfd6dc;border-radius:24px;cursor:pointer;transition:.2s}
.slider:before{content:"";position:absolute;width:18px;height:18px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.2s}
.switch input:checked+.slider{background:var(--acc)}
.switch input:checked+.slider:before{transform:translateX(18px)}
#log{background:#111;color:#9f9;font:12px monospace;padding:8px;height:200px;overflow:auto;white-space:pre-wrap;border-radius:6px}
.bar{height:8px;background:#eef1f4;border-radius:4px;overflow:hidden;margin-top:6px}
.bar>div{height:100%;background:var(--acc)}
.bypass-vis{font-size:12px;color:var(--mut);margin-top:4px}
/* ---- energy charts ---- */
#energy{--surface-1:#fcfcfb;--text-primary:#0b0b0b;--text-secondary:#52514e;--grid:#e6e5e1;
 --series-1:#2a78d6;--series-2:#eb6834;--series-3:#1baf7a;--series-4:#eda100}
.ev-seg{display:inline-flex;border:1px solid #c8d0d7;border-radius:7px;overflow:hidden}
.ev-seg button{border:0;background:#fff;padding:6px 12px;font-size:13px;cursor:pointer;border-right:1px solid #c8d0d7}
.ev-seg button:last-child{border-right:0}
.ev-seg button.on{background:#16202a;color:#fff}
.ev-tiles{display:grid;grid-template-columns:repeat(5,1fr);gap:10px;margin:12px 0}
@media(max-width:800px){.ev-tiles{grid-template-columns:repeat(2,1fr)}}
.ev-tile{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px}
.ev-tile .lbl{font-size:12px;color:var(--text-secondary);display:flex;align-items:center;gap:6px}
.ev-tile .v{font-size:28px;font-weight:700;color:var(--text-primary);margin-top:4px}
.ev-tile .v small{font-size:14px;font-weight:400;color:var(--text-secondary)}
.ev-tile .s{font-size:12px;color:var(--text-secondary);margin-top:2px}
.ev-key{display:inline-block;width:10px;height:10px;border-radius:2px}
.ev-chart{position:relative;background:var(--surface-1);border-radius:6px}
.ev-chart svg{width:100%;height:auto;display:block}
.ev-chart text{font:12px system-ui,Segoe UI,Arial,sans-serif;fill:var(--text-secondary)}
.ev-legend{display:flex;gap:14px;flex-wrap:wrap;font-size:12px;color:var(--text-secondary);margin-bottom:6px}
.ev-legend span{display:inline-flex;align-items:center;gap:6px}
.ev-legend i{display:inline-block;width:16px;height:2px;border-radius:1px}
.ev-legend i.box{width:10px;height:10px;border-radius:2px}
.ev-tip{position:absolute;pointer-events:none;background:#fff;border:1px solid var(--line);border-radius:6px;
 box-shadow:0 2px 8px rgba(0,0,0,.12);padding:7px 9px;font-size:12px;display:none;min-width:150px;z-index:5}
.ev-tip .h{color:var(--text-secondary);margin-bottom:4px}
.ev-tip .r{display:flex;align-items:center;gap:6px;margin:2px 0;color:var(--text-secondary)}
.ev-tip .r b{color:var(--text-primary);margin-left:auto;padding-left:10px}
.ev-tip .r i{display:inline-block;width:12px;height:2px}
.toast{position:fixed;top:60px;right:14px;background:#16202a;color:#fff;padding:8px 14px;border-radius:6px;font-size:13px;display:none}
</style></head><body>
<header>
  <div><h1>CH-HRV3.5KDC — Ventilation</h1><div class="sub" id="hdrSub">HKL-EA8 · Modbus RTU</div></div>
  <span class="pill"><span class="dot bad" id="linkDot"></span><span id="linkTxt">connecting…</span></span>
  <span class="pill" id="hdrState">—</span>
  <span class="pill" id="hdrTime">—</span>
  <span class="sp"></span>
  <a href="#" onclick="show('manual');return false">Manual</a>
  <a href="/relays">Relays</a>
</header>

<nav id="nav">
  <button data-t="dash" class="on">Dashboard</button>
  <button data-t="settings">Unit settings</button>
  <button data-t="auto">Automation</button>
  <button data-t="energy">Energy</button>
  <button data-t="adv">Advanced / Modbus</button>
  <button data-t="manual">Manual</button>
</nav>

<main>
<!-- ================= DASHBOARD ================= -->
<section class="tab on" id="dash">
  <div class="temps" style="margin-bottom:12px">
    <div class="t"><div class="lbl">Return air (RA) <span class="reg">R12</span></div><div class="big" id="tRA">--</div><div class="mut">from the rooms</div></div>
    <div class="t"><div class="lbl">Fresh air (FR) <span class="reg">R15</span></div><div class="big" id="tFR">--</div><div class="mut">intake from outdoors</div></div>
    <div class="t"><div class="lbl">Supply air (SA) <span class="reg">R14</span></div><div class="big" id="tSA">--</div><div class="mut">into the rooms</div></div>
    <div class="t"><div class="lbl">Exhaust air (OA) <span class="reg">R13</span></div><div class="big" id="tOA">--</div><div class="mut">out to outdoors</div></div>
  </div>
  <div class="grid">
    <div class="card">
      <h3>Power <span class="reg">R9</span></h3>
      <div class="power">
        <button class="pwr" id="pwr" onclick="togglePower()">⏻</button>
        <div><div class="big" id="pwrTxt" style="font-size:24px">--</div>
        <div class="mut" id="modeTxt"></div></div>
      </div>
      <div class="row mut" style="margin-top:12px">External on/off input <span class="reg">R16</span>: <b id="extIn">--</b></div>
    </div>
    <div class="card">
      <h3>Air cond <span class="reg">relay 1 · R18 B1</span></h3>
      <div class="row" style="justify-content:space-between;margin-top:0">
        <div class="big" id="acBig" style="font-size:24px">--</div>
        <label class="row" style="margin:0;cursor:pointer" title="Turn air cond on or off. The other settings are in Automation.">
          <span class="mut" id="acDashLbl">Off</span>
          <span class="switch"><input type="checkbox" id="acDashOn" onchange="acToggle(this)"><span class="slider"></span></span></label>
      </div>
      <div class="mut" id="acWhy"></div>
      <div class="flags" style="margin-top:8px" id="acRelay"></div>
      <table style="margin-top:8px">
        <tr><td>Cools when</td><td id="acTgt">--</td></tr>
        <tr><td>Relay state for</td><td id="acSince">--</td></tr>
      </table>
      <div class="note warn" id="acWarn" style="display:none"></div>
      <div class="row"><a href="#" onclick="show('auto');$('acCard').scrollIntoView({behavior:'smooth'});return false">Settings</a></div>
    </div>
    <div class="card" style="grid-column:span 2;min-width:0">
      <h3>Fan speed <span><span class="reg">R10</span> <span class="reg">R11</span></span></h3>
      <div class="row"><label><input type="checkbox" id="linkFans" checked onchange="localStorage.linkFans=this.checked?1:0;renderFans()"> Same speed for both fans</label></div>
      <div><b>Supply</b> <span class="mut" id="supCode"></span></div>
      <div class="speeds" id="spSup"></div>
      <div id="exhBlock"><b>Exhaust</b> <span class="mut" id="exhCode"></span>
      <div class="speeds" id="spExh"></div></div>
      <div class="row" style="margin-top:8px">
        <span class="mut">Presets:</span>
        <button class="btn" onclick="preset(2,2)">Night (2)</button>
        <button class="btn" onclick="preset(5,5)">Normal (5)</button>
        <button class="btn" onclick="preset(10,10)">Boost (10)</button>
        <button class="btn" onclick="preset(4,6)">Overpressure (S4 / E6)</button>
      </div>
    </div>
    <div class="card">
      <h3>Status <span class="reg">R18 / R19 / R22</span></h3>
      <div class="flags" id="flags"></div>
      <table style="margin-top:10px">
        <tr><td>Bypass window</td><td id="bpWin">--</td></tr>
        <tr><td>Electric heater stage</td><td id="heater">--</td></tr>
        <tr><td>Defrost mode</td><td id="dMode">--</td></tr>
        <tr><td>CO₂ on/off signal <span class="reg">R17</span></td><td id="co2sig">--</td></tr>
      </table>
    </div>
    <div class="card">
      <h3>Errors <span class="reg">R20</span></h3>
      <div class="flags" id="errs"></div>
      <div class="mut" style="margin-top:8px">Bits: B2 outdoor sensor, B3 defrost sensor, B4 room sensor, B5 EEPROM.</div>
    </div>
    <div class="card">
      <h3>Heat recovery now</h3>
      <div class="big" id="eff">--</div>
      <div class="bar"><div id="effBar" style="width:0"></div></div>
      <table style="margin-top:8px">
        <tr><td>Heat kept</td><td id="liveKept">--</td></tr>
        <tr><td>Heat lost</td><td id="liveLost">--</td></tr>
        <tr><td>Fans</td><td id="liveFan">--</td></tr>
        <tr><td>Exhaust side</td><td id="effExh">--</td></tr>
        <tr><td>Free cooling</td><td id="liveCool">--</td></tr>
      </table>
      <div class="mut" style="margin-top:6px">Efficiency = (SA − FR) / (RA − FR), supply side. Exhaust side = (RA − OA) / (RA − FR). Free cooling = 0.335 × supply airflow × (RA − FR) while the bypass is open. Power uses the airflow table in Energy.</div>
    </div>
  </div>
</section>

<!-- ================= SETTINGS ================= -->
<section class="tab" id="settings">
  <div class="note">Values are read from the unit. Changed fields are highlighted. <b>Apply</b> writes only the changed registers, then reads them back to confirm.</div>
  <div class="card">
    <table id="setTbl"></table>
    <div class="row" style="margin-top:10px">
      <button class="btn pri" onclick="applySettings()">Apply changes</button>
      <button class="btn" onclick="renderSettings()">Revert</button>
      <button class="btn" onclick="restoreDefaults()">Fill factory defaults</button>
      <span class="mut" id="setMsg"></span>
    </div>
  </div>
  <div class="grid" style="margin-top:12px">
    <div class="card">
      <h3>Bypass preview</h3>
      <div id="bpPreview"></div>
      <div class="bypass-vis">Bypass opens when X ≤ FR (outdoor) &lt; X + Y (manual p.19). Not used when SW4-2 is ON (bypass by dry contact, see Air cond in Automation).</div>
    </div>
    <div class="card">
      <h3>Restricted <span class="reg">R8 / R21</span></h3>
      <table>
        <tr><td>Modbus address</td><td><b id="curAddr">--</b></td><td><input type="number" min="1" max="16" id="newAddr"></td><td><button class="btn dng" onclick="changeAddr()">Change</button></td></tr>
        <tr><td>Model selection</td><td><b id="model">--</b></td><td colspan="2" class="mut">Read-only here. A wrong model changes fan curves.</td></tr>
      </table>
    </div>
  </div>
</section>

<!-- ================= AUTOMATION ================= -->
<section class="tab" id="auto">
  <div class="note warn">These features run on the ESP32, not on the HRV. They work only while the controller is powered and connected.</div>
  <div class="grid">
    <div class="card">
      <h3>Auto-restart after power loss</h3>
      <div class="row"><label class="switch"><input type="checkbox" id="rstOn"><span class="slider"></span></label> <b>Enabled</b></div>
      <div class="mut">The controller remembers the last state (on/off and fan speeds), including changes made on the wall controller. When the link comes back after an outage and the unit differs from that state, the controller writes it back.</div>
      <table style="margin-top:8px">
        <tr><td>Stored state</td><td id="stored">--</td></tr>
        <tr><td>Last restore</td><td id="lastRst">never</td></tr>
        <tr><td>Restore delay after link up</td><td><input type="number" id="rstDelay" min="0" max="600"> s</td></tr>
      </table>
      <div class="row"><button class="btn pri" onclick="saveRst()">Save</button></div>
    </div>

    <div class="card" id="acCard">
      <h3>Air cond (free cooling by bypass)</h3>
      <div class="row"><label class="switch"><input type="checkbox" id="acOn"><span class="slider"></span></label> <b>Enabled</b></div>
      <div class="row" id="acAutoSt"></div>
      <div class="mut">Closes relay 1 (bypass dry contact) when the room is too warm and the outdoor air is cool enough. Fresh air then goes straight into the rooms past the heat exchanger.
        Air cond wins over the weekly timer: while cooling is needed, the unit stays on even if the timer says OFF (and is switched on if it is off). When cooling is no longer needed, the timer is in charge again.
        Turning the unit OFF by hand stops cooling until the next timer ON or a manual ON.</div>
      <table style="margin-top:8px">
        <tr><td>Desired room temperature (RA)</td><td><input type="number" id="acTgtIn" step="1" min="10" max="35"> °C</td></tr>
        <tr><td>Outdoor (FR) cooler than room by at least</td><td><input type="number" id="acD" step="1" min="0" max="15"> °C</td></tr>
        <tr><td>Hysteresis</td><td><input type="number" id="acHyst" step="1" min="0" max="5"> °C</td></tr>
        <tr><td>Min hold time</td><td><input type="number" id="acHold" min="0" max="240"> min</td></tr>
        <tr><td><label><input type="checkbox" id="acHrs" onchange="acHrsUi()"> Only between</label></td>
          <td style="white-space:nowrap"><input type="time" id="acFrom"> – <input type="time" id="acTo"></td></tr>
      </table>
      <div class="mut" style="margin-top:6px">ON: RA &gt; desired and FR ≤ RA − delta. OFF: RA ≤ desired − hysteresis or FR &gt; RA − delta + hysteresis.
        The relay opens on a sensor error (R20), when the link is lost, and after a controller restart.</div>
      <div class="note warn">Needs DIP switch SW4-2 = ON (bypass by dry contact). With it, the unit runs the fans at high speed while the bypass is open and does not use its own bypass window (R2/R3). Relay 1 is locked on the Relays page while this is enabled.</div>
      <div class="row"><button class="btn pri" onclick="saveAc()">Save</button></div>
    </div>

    <div class="card" style="grid-column:span 2;min-width:0">
      <h3>Fan control by outdoor (FR) / return (RA) temperature</h3>
      <div class="row"><label class="switch"><input type="checkbox" id="rulesOn"><span class="slider"></span></label> <b>Enabled</b>
        <span class="mut">— speeds change only when another rule takes over, so a manual change stays until then. Only while the unit is ON.</span></div>
      <div class="mut" style="margin:6px 0">Rules are checked top to bottom; the first match sets the speeds. Ranges are “from ≤ value &lt; to”, an empty field means any.</div>
      <div style="overflow-x:auto"><table id="rules"></table></div>
      <div class="row">
        <button class="btn" onclick="addRule()">+ Add rule</button>
        Hysteresis <input type="number" id="hyst" step="0.5" min="0" max="5"> °C
        Min hold time <input type="number" id="hold" min="0" max="240"> min
        No rule matches: <select id="fb"><option value="0">keep current speed</option><option value="1">Normal (5/5)</option></select>
        <button class="btn pri" onclick="saveRules()">Save rules</button>
      </div>
    </div>
  </div>

  <div class="card" style="margin-top:12px">
    <h3>Weekly timer <span class="mut" style="text-transform:none;letter-spacing:0" id="clock"></span></h3>
    <div class="row"><label class="switch"><input type="checkbox" id="timerOn" onchange="renderTimer()"><span class="slider"></span></label> <b>Enabled</b>
      <span class="mut">— up to 2 ON/OFF periods per day. An OFF time earlier than the ON time means OFF on the next day (e.g. ON 18:00, OFF 09:00). At ON time the unit turns on with the fan speeds last set on the Dashboard (or by the temperature rules, if enabled); at OFF time it turns off. Saving applies the schedule immediately.</span></div>
    <div class="note warn">Time comes from NTP (Europe/Kyiv). If the time is unknown, the timer does nothing. Do not run the wall controller's own weekly timer at the same time.</div>
    <div style="overflow-x:auto"><table id="timerTbl"></table></div>
    <div class="row" style="margin-top:8px">
      <button class="btn" onclick="copyDay(0,[1,2,3,4])">Copy Monday → Tue–Fri</button>
      <button class="btn" onclick="copyDay(0,[1,2,3,4,5,6])">Copy Monday → all days</button>
      <button class="btn" onclick="copyDay(5,[6])">Copy Saturday → Sunday</button>
      <span style="flex:1"></span>
      <button class="btn pri" onclick="saveTimer()">Save timer</button>
    </div>
    <div class="row mut">Manual change: <select id="rev"><option value="0">keep until the next timer event</option><option value="60">revert after 1 h</option><option value="120">revert after 2 h</option><option value="240">revert after 4 h</option></select></div>
    <div class="row">Next event: <b id="nextEv">—</b> <span class="mut" style="margin-left:14px">Last timer action:</span> <b id="tmrLast">—</b></div>
  </div>
</section>

<!-- ================= ENERGY ================= -->
<section class="tab" id="energy">
  <div class="row" style="justify-content:space-between">
    <div class="ev-seg" id="evRange">
      <button data-r="24h" class="on">24 h</button><button data-r="7d">7 days</button>
      <button data-r="30d">30 days</button><button data-r="12m">12 months</button>
    </div>
    <div class="row" style="margin:0">
      <span class="mut" id="evStore"></span>
      <button class="btn" onclick="location.href='/api/csv?range='+EV.range">Download CSV</button>
      <button class="btn dng" onclick="clearHistory()">Clear history</button>
    </div>
  </div>
  <div class="note warn" id="evMsg" style="display:none"></div>
  <div class="ev-tiles" id="evTiles"></div>
  <div class="card">
    <h3>Temperatures <span class="mut" style="text-transform:none;letter-spacing:0" id="evTempSub"></span></h3>
    <div class="ev-legend" id="evTempLeg"></div>
    <div class="ev-chart" id="evTemp"></div>
  </div>
  <div class="card" style="margin-top:12px">
    <h3>Heat kept vs lost <button class="btn" id="evTblBtn" onclick="evToggleTable()">Show table</button></h3>
    <div class="ev-legend" id="evEnLeg"></div>
    <div class="ev-chart" id="evEn"></div>
    <div id="evTbl" style="display:none;max-height:260px;overflow:auto;margin-top:8px"></div>
  </div>
  <div class="card" style="margin-top:12px">
    <h3>Heat recovery efficiency <span class="mut" style="text-transform:none;letter-spacing:0">heating time only; bypass and defrost excluded</span></h3>
    <div class="ev-chart" id="evEff"></div>
  </div>
  <div class="grid" style="margin-top:12px">
    <div class="card">
      <h3>Prices</h3>
      <table>
        <tr><td>Heat price <span class="mut">(what 1 kWh of heating costs you)</span></td><td><input type="number" id="evHeatP" step="0.01" onchange="renderEnergy()"> ₴/kWh</td></tr>
        <tr><td>Electricity price <span class="mut">(fans)</span></td><td><input type="number" id="evElP" step="0.01" onchange="renderEnergy()"> ₴/kWh</td></tr>
      </table>
      <div class="row"><button class="btn pri" onclick="saveEnergy()">Save</button></div>
      <div class="mut" style="margin-top:8px">Calculated every poll from the four temperatures and the current fan speeds (R10/R11):<br>
        kept = 0.335 × supply airflow × (SA − FR)<br>
        lost = 0.335 × supply airflow × (RA − SA)<br>
        kept + lost is what heating the outdoor air (FR) to room temperature would take without the HRV.
        Counted only while heating (FR below RA), not during defrost (R18 B3) or with the bypass open (R18 B1).
        Free cooling while the bypass is open is shown live on the Dashboard; it is not stored in the history.
        Only sensible heat; moisture recovery is not counted, so real savings are a bit higher.</div>
    </div>
    <div class="card">
      <h3>Airflow and fan power per speed</h3>
      <div class="mut" style="margin-bottom:6px">Defaults are estimates (straight line to 350 m³/h). Replace with measured values if you have them. Fan W is for both fans together.</div>
      <div style="max-height:300px;overflow:auto"><table id="evFlowTbl"></table></div>
      <div class="row"><button class="btn pri" onclick="saveEnergy()">Save</button> <button class="btn" onclick="flowDefaults()">Fill defaults</button></div>
    </div>
  </div>
</section>

<!-- ================= ADVANCED ================= -->
<section class="tab" id="adv">
  <div class="grid">
    <div class="card">
      <h3>Link settings</h3>
      <table>
        <tr><td>Slave address</td><td><input type="number" id="lAddr" min="1" max="247"></td></tr>
        <tr><td>Baud</td><td><select id="lBaud"><option>1200</option><option>2400</option><option>4800</option><option>9600</option><option>19200</option><option>38400</option><option>57600</option><option>115200</option></select></td></tr>
        <tr><td>Format</td><td><select id="lFmt"><option value="0">8N1</option><option value="1">8E1</option><option value="2">8O1</option><option value="3">8N2</option></select></td></tr>
        <tr><td>Poll interval</td><td><input type="number" id="lPoll" min="1" max="60"> s</td></tr>
        <tr><td>Timeout</td><td><input type="number" id="lTo" min="50" max="2000"> ms</td></tr>
        <tr><td>Write function</td><td><select id="lWfc"><option value="6">FC06 single register</option><option value="16">FC16 multiple registers</option></select></td></tr>
      </table>
      <div class="row"><button class="btn pri" onclick="saveLink()">Save</button></div>
      <table style="margin-top:8px">
        <tr><td>Requests OK / failed</td><td id="cnt">--</td></tr>
        <tr><td>Last error</td><td id="lastErr">--</td></tr>
        <tr><td>Read mode</td><td id="rmode">--</td></tr>
        <tr><td>Uptime</td><td id="uptime">--</td></tr>
      </table>
    </div>
    <div class="card">
      <h3>Raw register access</h3>
      <div class="row">Reg <input type="number" id="rawReg" value="12" min="0" max="65535">
        Count <input type="number" id="rawCnt" value="1" min="1" max="30" style="width:60px"></div>
      <div class="row"><button class="btn" onclick="raw('r3')">Read FC03</button> <button class="btn" onclick="raw('r4')">Read FC04</button></div>
      <div class="row">Value(s) <input type="text" id="rawVal" value="0" style="width:140px">
        <button class="btn dng" onclick="raw('w6')">Write FC06</button> <button class="btn dng" onclick="raw('w16')">Write FC16</button></div>
      <div class="mut">FC16 takes comma-separated values starting at Reg. Writes ask for confirmation.</div>
      <div class="mut" id="rawOut" style="font-family:monospace;margin-top:8px;white-space:pre-wrap"></div>
    </div>
  </div>
  <div class="card" style="margin-top:12px">
    <h3>All registers (live) <span class="mut" style="text-transform:none;letter-spacing:0">refreshed every 2 s · read-only; use “Load to raw access” to read or write one register</span></h3>
    <table id="regTbl"></table>
  </div>
  <div class="card" style="margin-top:12px">
    <h3>Log <button class="btn" onclick="loadLog()">Refresh</button></h3>
    <div id="log"></div>
  </div>
</section>

<!-- ================= MANUAL ================= -->
<section class="tab" id="manual">
  <div class="grid">
    <div class="card">
      <h3>DIP switch SW4 (p.20)</h3>
      <div class="note warn" style="margin-top:0">Turn the power off before changing.</div>
      <table>
        <tr><th>Switch</th><th>OFF</th><th>ON</th></tr>
        <tr><td>SW4-1</td><td>Standard defrost using the exhaust fan</td><td>Defrost with an electric heater on the outdoor-air side (parameter 01 forced to 0)</td></tr>
        <tr><td>SW4-2</td><td>Automatic bypass</td><td>Manual bypass by dry contact (free cooling); fans at high speed</td></tr>
        <tr><td>SW4-3</td><td>CO₂ sensor</td><td>Humidity/temperature sensor (E3 if missing)</td></tr>
        <tr><td>SW4-4</td><td>Not used</td><td>Not used</td></tr>
      </table>
    </div>
    <div class="card">
      <h3>Display error codes (p.19)</h3>
      <table>
        <tr><th>Code</th><th>Error</th></tr>
        <tr><td>E1</td><td>Outdoor air temperature sensor</td></tr>
        <tr><td>E2</td><td>EEPROM</td></tr>
        <tr><td>E3</td><td>Exhaust air temperature sensor</td></tr>
        <tr><td>E4</td><td>Defrost sensor</td></tr>
        <tr><td>E5</td><td>Communication</td></tr>
        <tr><td>E6</td><td>Supply air temperature sensor</td></tr>
        <tr><td>E7</td><td>Exhaust fan</td></tr>
        <tr><td>E8</td><td>Supply fan</td></tr>
      </table>
    </div>
    <div class="card">
      <h3>Specifications (p.5)</h3>
      <table>
        <tr><td>Airflow</td><td>350 m³/h (100 l/s)</td></tr>
        <tr><td>Enthalpy efficiency</td><td>69 % heat / 66 % cool</td></tr>
        <tr><td>Temperature efficiency</td><td>75 %</td></tr>
        <tr><td>Sound pressure</td><td>28 dB(A)</td></tr>
        <tr><td>Power</td><td>220 V / 1 ph / 50 Hz, 112 W</td></tr>
        <tr><td>Cables</td><td>power 2×1.5 mm², control 2×0.5 mm²</td></tr>
        <tr><td>Duct</td><td>Ø150 mm</td></tr>
        <tr><td>Dimensions</td><td>804 × 270 × 882 mm, 37 kg</td></tr>
        <tr><td>Fans</td><td>DC, 10 speeds each</td></tr>
        <tr><td>Inputs</td><td>External on/off; fire alarm (dry contact, closed = off)</td></tr>
        <tr><td>Model code (R21)</td><td>4</td></tr>
      </table>
    </div>
    <div class="card">
      <h3>RS485 wiring</h3>
      <table>
        <tr><td>Main board terminal</td><td>12V | A | B | GND</td></tr>
        <tr><td>ESP32 UART</td><td>RX GPIO14 / TX GPIO13</td></tr>
        <tr><td>Link</td><td>Modbus RTU, addr 14, 4800 8N1</td></tr>
      </table>
      <div class="mut" style="margin-top:6px">Leave 12V unconnected. If there is no reply, swap A/B.</div>
    </div>
  </div>
</section>
</main>
<div class="toast" id="toast"></div>

<script>
const $=id=>document.getElementById(id);
const SPEED_CODES=[0,2,3,5,8,9,10,11,12,13,14];
const lvl=c=>SPEED_CODES.indexOf(c);
const s16=v=>v>32767?v-65536:v;
const DAYS=['Mon','Tue','Wed','Thu','Fri','Sat','Sun'];
const FLOW_DEF=[0,35,70,105,140,175,210,245,280,315,350],FANW_DEF=[0,8,11,15,20,27,36,48,63,85,112];
const META={
 0:{n:'Automatic reloading'},1:{n:'Heater connection'},
 2:{n:'Bypass opening temperature X',u:'°C',min:5,max:30,def:19},
 3:{n:'Bypass temperature band Y',u:'°C',min:2,max:15,def:3},
 4:{n:'Defrost interval',u:'min',min:15,max:99,def:30},
 5:{n:'Outdoor temp that triggers defrost',u:'°C',min:-9,max:5,def:-1},
 6:{n:'Defrost duration',u:'min',min:2,max:20,def:10},
 7:{n:'CO₂ setpoint',u:'ppm',min:392,max:1960,def:1000,co2:1},
 8:{n:'Modbus address'},9:{n:'Unit on/off'},10:{n:'Supply fan speed'},11:{n:'Exhaust fan speed'},
 12:{n:'Return air (RA) temperature'},13:{n:'Exhaust air (OA) temperature'},14:{n:'Supply air (SA) temperature'},15:{n:'Fresh air (FR) temperature'},
 16:{n:'External on/off input'},17:{n:'CO₂ on/off signal'},18:{n:'Status bits'},19:{n:'Electric heater stage'},
 20:{n:'Error bits'},21:{n:'Model selection'},22:{n:'Defrost mode'}};
let S=null,CFG=null,R=[],RULES=[],TIMER=[],TAB='dash';

function toast(t){const e=$('toast');e.textContent=t;e.style.display='block';clearTimeout(e._t);e._t=setTimeout(()=>e.style.display='none',2600)}
async function call(path,params,method){
  const u=path+(params?'?'+new URLSearchParams(params):'');
  try{const r=await fetch(u,{method:method||'POST'});const t=await r.text();let j;
    try{j=JSON.parse(t)}catch(e){j={ok:r.ok,err:t}}
    if(!r.ok&&typeof j==='object'&&!Array.isArray(j))j.ok=false;return j}
  catch(e){return{ok:false,err:'network error'}}}
const get=(p,q)=>call(p,q,'GET');
function done(j,okMsg){if(j.ok){if(okMsg)toast(okMsg);return true}toast('Failed: '+(j.err||'error'));return false}
const T_OFS=40;  // R12..R15: °C = raw − 40
const temp=r=>(!S||R[r]==null)?null:R[r]-T_OFS;
const fT=v=>v==null||isNaN(v)?'--':String(Math.round(v));
const unitOn=()=>R[9]===1;
const hm=m=>String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0');
const toMin=s=>{const p=(s||'0:0').split(':');return(+p[0])*60+(+p[1])};
function dur(s){if(s<0)return'—';if(s<90)return s+' s';if(s<5400)return Math.round(s/60)+' min';if(s<172800)return Math.round(s/3600)+' h';return Math.round(s/86400)+' days'}
const dt=t=>new Date(t*1000).toLocaleString('en-GB',{day:'numeric',month:'short',hour:'2-digit',minute:'2-digit'});

// ---------- tabs ----------
function show(t){if(!$(t))t='dash';TAB=t;history.replaceState(null,'','#'+t);document.querySelectorAll('.tab').forEach(s=>s.classList.toggle('on',s.id==t));
 document.querySelectorAll('nav button').forEach(b=>b.classList.toggle('on',b.dataset.t==t));
 if(t=='settings')renderSettings();
 if(t=='energy')loadEnergy();
 if(t=='adv'){renderRegs();loadLog()}}
$('nav').onclick=e=>{if(e.target.dataset.t)show(e.target.dataset.t)};

// ---------- polling ----------
async function poll(){
  const j=await get('/api/state');
  if(j&&j.r){S=j;R=j.r;renderAll()}
  else{$('linkDot').className='dot bad';$('linkTxt').textContent='controller not reachable'}
  setTimeout(poll,2000)}
function renderAll(){
  const L=S.link;
  $('linkDot').className='dot'+(L.ok?'':' bad');
  $('linkTxt').textContent=L.ok?`Link OK · polled ${L.ago<=1?'just now':L.ago+' s ago'}`:`No link · ${L.err}`;
  $('hdrSub').textContent=`HKL-EA8 · Modbus RTU addr ${L.addr} · ${L.baud} ${L.fmt}`;
  $('hdrState').textContent=R[9]==null?'Unit —':`Unit ${unitOn()?'ON':'OFF'} · S${lvl(R[10])} / E${lvl(R[11])}`;
  $('hdrTime').textContent=S.tv?new Date(S.t*1000).toLocaleString('en-GB',{weekday:'short',hour:'2-digit',minute:'2-digit'}):'time not synced';
  renderDash();
  // automation status
  $('stored').textContent=S.des.valid?`${S.des.on?'ON':'OFF'} · S${S.des.s} / E${S.des.e}`:'not known yet';
  $('lastRst').textContent=S.rst.last?dt(S.rst.last):'never'+(S.rst.pend>=0?` (check in ${S.rst.pend} s)`:'');
  document.querySelectorAll('#rules tr[data-i]').forEach(tr=>tr.classList.toggle('changed',S.rule.on&&+tr.dataset.i===S.rule.act));
  if(S.tv)$('clock').textContent='Controller time: '+new Date(S.t*1000).toLocaleString('en-GB');
  else $('clock').textContent='Controller time: not synced';
  nextEvent();
  $('tmrLast').textContent=S.tmr.last||'none since start';
  // advanced
  $('cnt').textContent=`${L.okc} / ${L.errc}`;
  $('lastErr').textContent=L.errAgo<0?'none':`${L.err}, ${dur(L.errAgo)} ago`;
  $('rmode').textContent=(L.single?'single registers':'block read R2–R21 + R22 single')+(L.echo?', transceiver echo handled':'');
  $('uptime').textContent=dur(S.up);
  if(TAB=='adv')renderRegs();
}

// ---------- dashboard ----------
async function togglePower(){if(R[9]==null)return;const j=await call('/api/power',{on:unitOn()?0:1});done(j,unitOn()?'Switching OFF':'Switching ON')}
function speedButtons(el,reg){
  let h=`<button class="stop ${R[reg]===0?'on':''}" onclick="setSpeed(${reg},0)">Stop</button>`;
  for(let i=1;i<=10;i++)h+=`<button class="${lvl(R[reg])==i?'on':''}" onclick="setSpeed(${reg},${i})">${i}</button>`;
  $(el).innerHTML=h}
async function setSpeed(reg,l){let s=lvl(R[10]),e=lvl(R[11]);
  if($('linkFans').checked){s=e=l}else if(reg==10)s=l;else e=l;
  if(s<0||e<0){toast('Current speed unknown');return}
  done(await call('/api/speed',{s,e}))}
async function preset(s,e){$('linkFans').checked=(s==e);localStorage.linkFans=s==e?1:0;renderFans();done(await call('/api/speed',{s,e}),`Speeds set: S${s} / E${e}`)}
function renderFans(){speedButtons('spSup',10);speedButtons('spExh',11);
  $('exhBlock').style.display=$('linkFans').checked?'none':'block';
  $('supCode').textContent=R[10]==null?'':`level ${lvl(R[10])} (code ${R[10]})`+($('linkFans').checked?' — applies to both fans':'');
  $('exhCode').textContent=R[11]==null?'':`level ${lvl(R[11])} (code ${R[11]})`}
const flag=(t,c)=>`<span class="flag ${c||''}">${t}</span>`;
const deg=v=>`${fT(v)}<span class="unit"> °C</span>`;
function renderDash(){
  $('tRA').innerHTML=deg(temp(12));$('tOA').innerHTML=deg(temp(13));$('tSA').innerHTML=deg(temp(14));$('tFR').innerHTML=deg(temp(15));
  $('pwr').classList.toggle('on',unitOn());$('pwrTxt').textContent=R[9]==null?'--':unitOn()?'ON':'OFF';
  $('modeTxt').textContent='Speed: '+(S.rule.on?'temperature rules'+(S.rule.act>=0?` (rule ${S.rule.act+1})`:''):'manual')+(S.tmr.on?' · timer on':'')+(S.ac&&S.ac.st==2?' · air cond cooling':'');
  $('extIn').textContent=R[16]==null?'--':R[16]?'closed':'open';
  renderFans();
  const b=R[18];
  $('flags').innerHTML=b==null?'--':(b&1?flag('FIRE ALARM','err'):flag('Fire alarm: no'))+
    (b&2?flag('Bypass OPEN','on'):'')+(b&4?flag('Bypass closed'):'')+(b&8?flag('Defrosting','warn'):flag('Defrost: idle'));
  $('bpWin').textContent=R[2]==null?'--':`${R[2]} – ${R[2]+R[3]} °C (FR now ${fT(temp(15))} °C)`;
  $('heater').textContent=R[19]??'--';$('dMode').textContent=R[22]??'--';$('co2sig').textContent=R[17]==null?'--':R[17]?'on':'off';
  const e=R[20],E=[[4,'Outdoor sensor'],[8,'Defrost sensor'],[16,'Room sensor'],[32,'EEPROM']];
  $('errs').innerHTML=e==null?'--':e?E.filter(x=>e&x[0]).map(x=>flag(x[1],'err')).join('')+(e&~60?flag('Other bits: 0x'+e.toString(16),'err'):''):flag('No errors','on');
  const ra=temp(12),oa=temp(13),sa=temp(14),fr=temp(15),byp=!!(b&2),dT=ra!=null&&fr!=null&&Math.abs(ra-fr)>=2&&unitOn()&&!byp;
  if(dT&&sa!=null){const ef=Math.round(100*(sa-fr)/(ra-fr));
    $('eff').innerHTML=ef+'<span class="unit"> %</span>';$('effBar').style.width=Math.max(0,Math.min(100,ef))+'%'}
  else{$('eff').innerHTML=(byp?'bypass':'--')+'<span class="unit">'+(byp?'':' %')+'</span>';$('effBar').style.width='0'}
  $('effExh').textContent=dT&&oa!=null?Math.round(100*(ra-oa)/(ra-fr))+' %':'--';
  const lv=S.live,kw=w=>w==null?'--':w>=1000?(w/1000).toFixed(2)+' kW':Math.round(w)+' W';
  $('liveKept').textContent=lv.heating?kw(lv.kept):'0 W (not heating)';$('liveLost').textContent=lv.heating?kw(lv.lost):'—';
  $('liveFan').textContent=kw(lv.fan);
  $('liveCool').textContent=lv.cool>0?kw(lv.cool):'— (bypass closed)';
  renderAcDash();
}

// ---------- air cond status (Dashboard card + Automation card) ----------
const AC_ST=['Off','Waiting','Cooling','Blocked'],AC_CLS=['','','on','err'];
function acStatus(){const a=S.ac;if(!a)return null;
  let warn='';
  // the unit reports the bypass in R18 B1; if the relay has been closed for a minute and the unit does not confirm, say so
  if(a.relay&&a.ago>=60&&R[18]!=null&&!(R[18]&2))warn='Relay 1 is closed but the unit does not report the bypass open (R18 B1). Check SW4-2 and the wiring.';
  if(!a.relay&&a.ago>=60&&R[18]!=null&&(R[18]&2))warn='The unit reports the bypass open although relay 1 is open (automatic bypass? check SW4-2).';
  return{txt:AC_ST[a.st]||'--',cls:AC_CLS[a.st]||'',why:a.why,warn}}
// Dashboard toggle: saves the stored air cond settings with only "on" changed
async function acToggle(el){const a=CFG&&CFG.ac;if(!a){el.checked=!el.checked;return}
  el.disabled=true;
  const ok=done(await call('/api/config',{section:'ac',on:el.checked?1:0,tgt:a.tgt,d:a.d,hyst:a.hyst,hold:a.hold,hrs:a.hrs,from:a.from,to:a.to}),
    el.checked?'Air cond enabled':'Air cond disabled');
  el.disabled=false;
  if(ok){a.on=el.checked?1:0;$('acOn').checked=el.checked;await refreshState()}else el.checked=!el.checked}
function renderAcDash(){const s=acStatus();if(!s)return;const a=S.ac;
  if(!$('acDashOn').disabled)$('acDashOn').checked=!!a.on;$('acDashLbl').textContent=a.on?'On':'Off';
  $('acBig').textContent=s.txt;$('acBig').style.color=a.st==2?'var(--acc)':a.st==3?'var(--err)':'';
  $('acWhy').textContent=s.why||'';
  $('acRelay').innerHTML=a.relay?flag('Relay 1 closed · bypass on','on'):flag('Relay 1 open');
  $('acSince').textContent=a.ago>=0?dur(a.ago):'—';
  $('acTgt').textContent=CFG&&CFG.ac?`room > ${CFG.ac.tgt} °C, FR ≤ RA − ${CFG.ac.d} °C`:'--';
  $('acWarn').style.display=s.warn?'block':'none';$('acWarn').textContent=s.warn;
  $('acAutoSt').innerHTML=flag(s.txt,s.cls)+(s.why?` <span class="mut">${s.why}</span>`:'')}

// ---------- unit settings ----------
const toDisp=(r,v)=>META[r].co2?Math.round(v*9.8):s16(v);
const toVal=(r,v)=>META[r].co2?Math.round(v/9.8):v;
function renderSettings(){
  let h='<tr><th>Reg</th><th>Parameter</th><th>Current</th><th>New value</th><th>Range</th><th>Default</th></tr>';
  for(const r of [2,3,4,5,6,7]){const m=META[r],cur=R[r]==null?null:toDisp(r,R[r]);
    h+=`<tr id="row${r}"><td><span class="reg">R${r}</span></td><td>${m.n}</td><td><b>${cur??'--'}</b> ${m.u}</td>`+
     `<td><input type="number" id="in${r}" value="${cur??''}" min="${m.min}" max="${m.max}" ${m.co2?'step="10"':''} oninput="markChg(${r})"> ${m.u}</td>`+
     `<td class="mut">${m.min} … ${m.max}</td><td class="mut">${m.def}</td></tr>`}
  $('setTbl').innerHTML=h;$('setMsg').textContent='';
  $('curAddr').textContent=R[8]??'--';if(!$('newAddr').value)$('newAddr').value=R[8]??'';
  $('model').textContent=R[21]==null?'--':R[21]+(R[21]==4?' (3.5KDC)':'');
  bpPreview()}
function markChg(r){const cur=R[r]==null?null:toDisp(r,R[r]);$('row'+r).classList.toggle('changed',$('in'+r).value!==''&&+$('in'+r).value!==cur);bpPreview()}
async function applySettings(){const q={},bad=[];
  for(const r of [2,3,4,5,6,7]){const m=META[r],s=$('in'+r).value;if(s==='')continue;const v=+s;
    if(R[r]!=null&&v===toDisp(r,R[r]))continue;
    if(v<m.min||v>m.max){bad.push(m.n);continue}
    q['r'+r]=toVal(r,v)}
  if(bad.length){$('setMsg').textContent='Out of range: '+bad.join(', ');return}
  if(!Object.keys(q).length){$('setMsg').textContent='Nothing changed';return}
  $('setMsg').textContent='Writing…';
  const j=await call('/api/write',q);
  const failed=(j.res||[]).filter(x=>!x.ok);
  await refreshState();renderSettings();
  $('setMsg').textContent=j.ok?`${(j.res||[]).length} register(s) written and verified`:
    'Failed: '+(failed.length?failed.map(x=>`R${x.r} (${x.err})`).join(', '):j.err)}
function restoreDefaults(){for(const r of [2,3,4,5,6,7]){$('in'+r).value=META[r].def;markChg(r)}}
function bpPreview(){const X=+$('in2').value,Y=+$('in3').value,o=temp(15);
  $('bpPreview').innerHTML=`<div class="big" style="font-size:24px">${X} – ${X+Y} °C</div>`+
   (o==null?'':`<div class="mut">Outdoor now ${fT(o)} °C → bypass would be <b>${(o>=X&&o<X+Y)?'OPEN':'closed'}</b></div>`)}
async function changeAddr(){const v=+$('newAddr').value;
  if(!(v>=1&&v<=16)){toast('Address must be 1..16');return}
  if(!confirm(`Change the unit's Modbus address to ${v}? The controller will switch to the new address.`))return;
  if(done(await call('/api/addr',{v}),'Address changed')){await loadConfig();renderSettings()}}
async function refreshState(){const j=await get('/api/state');if(j&&j.r){S=j;R=j.r;renderAll()}}

// ---------- automation: restore ----------
async function saveRst(){done(await call('/api/config',{section:'rst',on:$('rstOn').checked?1:0,delay:$('rstDelay').value}),'Auto-restart saved')}

// ---------- automation: rules ----------
function renderRules(){
  const opts=v=>[...Array(11).keys()].map(i=>`<option value="${i}" ${i==v?'selected':''}>${i==0?'stop':i}</option>`).join('');
  const num=(i,k,v)=>`<input type="number" step="0.5" style="width:58px" value="${v??''}" placeholder="any" onchange="RULES[${i}][${k}]=this.value===''?null:+this.value">`;
  let h='<tr><th>#</th><th>FR °C from – to</th><th>RA °C from – to</th><th>FR vs RA</th><th>Supply</th><th>Exhaust</th><th>Active</th><th></th></tr>';
  RULES.forEach((r,i)=>{
    h+=`<tr data-i="${i}"><td>${i+1}</td><td style="white-space:nowrap">${num(i,1,r[1])} – ${num(i,2,r[2])}</td><td style="white-space:nowrap">${num(i,3,r[3])} – ${num(i,4,r[4])}</td>`+
     `<td><select onchange="RULES[${i}][5]=+this.value;renderRules()"><option value="0" ${r[5]==0?'selected':''}>any</option>`+
     `<option value="1" ${r[5]==1?'selected':''}>FR cooler by ≥</option><option value="2" ${r[5]==2?'selected':''}>FR warmer</option></select>`+
     (r[5]==1?` <input type="number" step="0.5" style="width:56px" value="${r[6]??3}" onchange="RULES[${i}][6]=+this.value"> °C`:'')+`</td>`+
     `<td><select onchange="RULES[${i}][7]=+this.value">${opts(r[7])}</select></td><td><select onchange="RULES[${i}][8]=+this.value">${opts(r[8])}</select></td>`+
     `<td><input type="checkbox" ${r[0]?'checked':''} onchange="RULES[${i}][0]=this.checked?1:0"></td>`+
     `<td style="white-space:nowrap"><button class="btn" title="Move up" onclick="moveRule(${i})" ${i?'':'disabled'}>↑</button> <button class="btn" title="Delete" onclick="RULES.splice(${i},1);renderRules()">✕</button></td></tr>`});
  $('rules').innerHTML=h}
function addRule(){if(RULES.length>=8){toast('Up to 8 rules');return}RULES.push([1,null,null,null,null,0,3,5,5]);renderRules()}
function moveRule(i){const t=RULES[i];RULES[i]=RULES[i-1];RULES[i-1]=t;renderRules()}
async function saveRules(){const q={section:'rules',on:$('rulesOn').checked?1:0,hyst:$('hyst').value,hold:$('hold').value,fb:$('fb').value,n:RULES.length};
  RULES.forEach((r,i)=>q['r'+i]=r.map(v=>v==null?'':v).join(','));
  done(await call('/api/config',q),'Rules saved')}

// ---------- automation: weekly timer ----------
function renderTimer(){
  const dis=$('timerOn').checked?'':'disabled';
  let h='<tr><th>Day</th><th>Period 1</th><th>ON</th><th>OFF</th><th style="border-left:2px solid var(--line)">Period 2</th><th>ON</th><th>OFF</th></tr>';
  const today=S&&S.tv?(new Date(S.t*1000).getDay()+6)%7:-1;
  TIMER.forEach((d,i)=>{h+=`<tr ${i==today?'style="background:var(--acc2)"':''}><td><b>${DAYS[i]}</b>${i==today?' <span class="mut">today</span>':''}</td>`;
    d.forEach((p,k)=>{const f=x=>`onchange="tSet(${i},${k},'${x}',this)"`,off=dis||(!p[0]?'disabled':'');
      h+=`<td ${k?'style="border-left:2px solid var(--line)"':''}><input type="checkbox" ${p[0]?'checked':''} ${dis} ${f('en')}></td>`+
       `<td><input type="time" value="${hm(p[1])}" ${off} ${f('on')}></td><td style="white-space:nowrap"><input type="time" value="${hm(p[2])}" ${off} ${f('off')}>${p[0]&&p[2]<p[1]?` <span class="mut">next day (${DAYS[(i+1)%7]})</span>`:''}</td>`});
    h+='</tr>'});
  $('timerTbl').innerHTML=h;nextEvent()}
function tSet(d,k,key,el){const p=TIMER[d][k];
  if(key=='en')p[0]=el.checked?1:0;else if(key=='on')p[1]=toMin(el.value);else p[2]=toMin(el.value);
  if(p[0]&&p[2]==p[1]&&key!='en')toast('OFF time must differ from ON time');renderTimer()}
function copyDay(src,dst){dst.forEach(d=>TIMER[d]=JSON.parse(JSON.stringify(TIMER[src])));renderTimer();toast(`${DAYS[src]} copied — press Save timer`)}
function nextEvent(){
  if(!TIMER.length)return;
  if(!$('timerOn').checked){$('nextEv').textContent='timer disabled';return}
  if(!S||!S.tv){$('nextEv').textContent='controller time not synced';return}
  const now=new Date(S.t*1000),today=(now.getDay()+6)%7,cur=now.getHours()*60+now.getMinutes();
  for(let o=0;o<8;o++){const d=(today+o)%7,ev=[];
    TIMER[d].forEach(p=>{if(p[0]){ev.push([p[1],'ON']);if(p[2]>p[1])ev.push([p[2],'OFF'])}});
    TIMER[(d+6)%7].forEach(p=>{if(p[0]&&p[2]<p[1])ev.push([p[2],'OFF'])});   // overnight periods of the previous day
    ev.sort((a,b)=>a[0]-b[0]);
    for(const e of ev)if(o>0||e[0]>cur){$('nextEv').textContent=`${o==0?'today':o==1?'tomorrow':DAYS[d]} ${hm(e[0])} — ${e[1]}`;return}}
  $('nextEv').textContent='no periods set'}
async function saveTimer(){
  for(let d=0;d<7;d++)for(const p of TIMER[d])if(p[0]&&p[2]==p[1]){toast(`${DAYS[d]}: OFF must differ from ON`);return}
  const q={section:'tmr',on:$('timerOn').checked?1:0,rev:$('rev').value};
  TIMER.forEach((d,i)=>q['d'+i]=d.map(p=>p.join(',')).join(','));
  done(await call('/api/config',q),'Timer saved')}

// ---------- config load ----------
async function loadConfig(){
  const c=await get('/api/config');if(!c||!c.link)return;CFG=c;
  $('lAddr').value=c.link.addr;$('lBaud').value=c.link.baud;$('lFmt').value=c.link.fmt;$('lPoll').value=c.link.poll;
  $('lTo').value=c.link.to;$('lWfc').value=c.link.wfc;
  $('rstOn').checked=!!c.rst.on;$('rstDelay').value=c.rst.delay;
  $('rulesOn').checked=!!c.rules.on;$('hyst').value=c.rules.hyst;$('hold').value=c.rules.hold;$('fb').value=c.rules.fb;
  RULES=c.rules.list.map(r=>r.slice());renderRules();
  $('timerOn').checked=!!c.tmr.on;$('rev').value=c.tmr.rev;TIMER=c.tmr.d.map(d=>d.map(p=>p.slice()));renderTimer();
  $('evHeatP').value=c.en.hp;$('evElP').value=c.en.ep;renderFlowTbl(c.en.flow,c.en.fanw);
  const a=c.ac;$('acOn').checked=!!a.on;$('acTgtIn').value=a.tgt;$('acD').value=a.d;$('acHyst').value=a.hyst;$('acHold').value=a.hold;
  $('acHrs').checked=!!a.hrs;$('acFrom').value=hm(a.from);$('acTo').value=hm(a.to);acHrsUi()}
function acHrsUi(){$('acFrom').disabled=$('acTo').disabled=!$('acHrs').checked}
async function saveAc(){
  if($('acHrs').checked&&$('acFrom').value==$('acTo').value){toast('Start and end time must differ');return}
  if(done(await call('/api/config',{section:'ac',on:$('acOn').checked?1:0,tgt:Math.round($('acTgtIn').value),d:Math.round($('acD').value),hyst:Math.round($('acHyst').value),
    hold:Math.round($('acHold').value),hrs:$('acHrs').checked?1:0,from:toMin($('acFrom').value),to:toMin($('acTo').value)}),'Air cond saved'))loadConfig()}
async function saveLink(){
  if(!confirm('Apply new link settings? Wrong values stop communication with the unit.'))return;
  if(done(await call('/api/config',{section:'link',addr:$('lAddr').value,baud:$('lBaud').value,fmt:$('lFmt').value,
    poll:$('lPoll').value,to:$('lTo').value,wfc:$('lWfc').value}),'Link settings saved'))loadConfig()}

// ---------- advanced ----------
function renderRegs(){
  let h='<tr><th>Reg</th><th>Name</th><th>Raw (hex)</th><th>Value</th><th></th></tr>';
  for(let r=0;r<=22;r++){const m=META[r],v=R[r];
    h+=`<tr><td><span class="reg">R${r}</span></td><td>${m.n}</td>`+
     `<td style="font-family:monospace">${v==null?'--':'0x'+v.toString(16).toUpperCase().padStart(4,'0')}</td>`+
     `<td>${v==null?'--':m.co2?toDisp(r,v)+' ppm':s16(v)}</td>`+
     `<td><button class="btn" title="Copy this register and its value into Raw register access" onclick="useReg(${r},${v??0})">Load to raw access</button></td></tr>`}
  $('regTbl').innerHTML=h}
for(const id of ['rawReg','rawCnt','rawVal']){const v=localStorage[id];if(v!=null)$(id).value=v;$(id).addEventListener('change',()=>localStorage[id]=$(id).value)}
if(localStorage.rawOut)$('rawOut').textContent=localStorage.rawOut;
if(localStorage.linkFans==='0')$('linkFans').checked=false;
function useReg(r,v){$('rawReg').value=r;$('rawVal').value=v;$('rawCnt').value=1;
  for(const id of ['rawReg','rawCnt','rawVal'])localStorage[id]=$(id).value;
  $('rawReg').closest('.card').scrollIntoView({behavior:'smooth',block:'center'});$('rawVal').focus();$('rawVal').select();
  toast(`R${r} loaded into Raw register access`)}
async function raw(op){
  for(const id of ['rawReg','rawCnt','rawVal'])localStorage[id]=$(id).value;
  const q={op,reg:$('rawReg').value};
  if(op[0]=='r')q.cnt=$('rawCnt').value;
  else{if(!confirm(`Write ${$('rawVal').value} to register ${q.reg} with ${op=='w6'?'FC06':'FC16'}?`))return;
    if(op=='w6')q.val=$('rawVal').value;else q.vals=$('rawVal').value}
  const j=await call('/api/raw',q);
  $('rawOut').textContent=(j.ok?(op[0]=='r'?'Values: '+j.vals.map((v,i)=>`R${+q.reg+i}=${v} (${s16(v)})`).join(', '):'Write OK'):'Error: '+j.err)+
    `\nTX: ${j.tx||''}\nRX: ${j.rx||'-'}`;localStorage.rawOut=$('rawOut').textContent}
async function loadLog(){const j=await get('/api/log');if(Array.isArray(j)){const l=$('log');l.textContent=j.join('\n');l.scrollTop=1e9}}

// ---------- energy ----------
const EV={range:'24h',data:null};
const C1='var(--series-1)',C2='var(--series-2)',C3='var(--series-3)',C4='var(--series-4)';
const evF=(t,o)=>new Date(t).toLocaleString('en-GB',o);
const EV_FMT={
  '24h':{tx:t=>evF(t,{hour:'2-digit',minute:'2-digit'}),tt:t=>evF(t,{weekday:'short',hour:'2-digit',minute:'2-digit'}),
         bx:t=>evF(t,{hour:'2-digit'}),bt:t=>evF(t,{hour:'2-digit',minute:'2-digit'})+' – '+evF(t+36e5,{hour:'2-digit',minute:'2-digit'})},
  '7d': {tx:t=>evF(t,{weekday:'short',day:'numeric'}),tt:t=>evF(t,{weekday:'short',day:'numeric',hour:'2-digit',minute:'2-digit'}),
         bx:t=>evF(t,{weekday:'short',day:'numeric'}),bt:t=>evF(t,{weekday:'long',day:'numeric',month:'short'})},
  '30d':{tx:t=>evF(t,{day:'numeric',month:'short'}),tt:t=>evF(t,{weekday:'short',day:'numeric',month:'short'}),
         bx:t=>evF(t,{day:'numeric',month:'short'}),bt:t=>evF(t,{weekday:'short',day:'numeric',month:'short'})},
  '12m':{tx:t=>evF(t,{month:'short'}),tt:t=>evF(t,{day:'numeric',month:'short',year:'numeric'}),
         bx:t=>evF(t,{month:'short'}),bt:t=>evF(t,{month:'long',year:'numeric'})}};
function evTicks(lo,hi,n){if(hi-lo<1e-9){lo-=1;hi+=1}
  const raw=(hi-lo)/n,p=10**Math.floor(Math.log10(raw)),st=[1,2,2.5,5,10].map(m=>m*p).find(x=>x>=raw);
  const r=[];for(let v=Math.floor(lo/st)*st;v<=Math.ceil(hi/st)*st+st/2;v+=st)r.push(+v.toFixed(6));return r}
function evEl(tag,cls,txt){const e=document.createElement(tag);if(cls)e.className=cls;if(txt!=null)e.textContent=txt;return e}
function evTipRow(color,name,val,box){const r=evEl('div','r'),k=evEl('i');k.style.background=color;
  if(box){k.style.width='10px';k.style.height='10px';k.style.borderRadius='2px'}
  r.append(k,evEl('span',null,name),evEl('b',null,val));return r}
function evPlaceTip(tip,svg,xVb,W){const r=svg.getBoundingClientRect(),px=xVb/W*r.width;
  tip.style.display='block';const w=tip.offsetWidth;tip.style.left=(px+14+w>r.width?px-14-w:px+14)+'px';tip.style.top='8px'}
function evLegend(id,items){$(id).replaceChildren(...items.map(it=>{const s=evEl('span'),k=evEl('i',it.box?'box':'');
  k.style.background=it.dash?`repeating-linear-gradient(90deg,${it.color} 0 4px,transparent 4px 7px)`:it.color;
  s.append(k,document.createTextNode(it.name));return s}))}
function evEmpty(el,msg){el.replaceChildren(evEl('div','mut',msg));el.firstChild.style.padding='30px';el.firstChild.style.textAlign='center'}

function evLine(el,o){
  const W=960,H=o.h||260,ml=40,mr=o.endLabels?96:16,mt=12,mb=26,iw=W-ml-mr,ih=H-mt-mb,n=o.x.length;
  const all=o.series.flatMap(s=>s.v).filter(v=>v!=null);
  if(!all.length){evEmpty(el,'No data for this period yet');return}
  if(o.ref)all.push(o.ref.v);
  const tk=o.fixedY?evTicks(o.fixedY[0],o.fixedY[1],5):evTicks(Math.min(...all),Math.max(...all),5);
  const lo=tk[0],hi=tk[tk.length-1],X=i=>ml+(n<2?iw/2:i*iw/(n-1)),Y=v=>mt+ih-(v-lo)/(hi-lo)*ih;
  let s=`<svg viewBox="0 0 ${W} ${H}" role="img" aria-label="${o.aria}">`;
  for(const v of tk)s+=`<line x1="${ml}" x2="${ml+iw}" y1="${Y(v)}" y2="${Y(v)}" stroke="var(--grid)"/><text x="${ml-8}" y="${Y(v)+4}" text-anchor="end">${v}${o.unit}</text>`;
  const nt=n<=13?n:7;
  for(let k=0;k<nt;k++){const i=Math.round(k*(n-1)/Math.max(1,nt-1));
    s+=`<text x="${X(i)}" y="${H-6}" text-anchor="${nt>1&&k==0?'start':nt>1&&k==nt-1?'end':'middle'}">${o.fmtX(o.x[i])}</text>`}
  if(o.ref)s+=`<line x1="${ml}" x2="${ml+iw}" y1="${Y(o.ref.v)}" y2="${Y(o.ref.v)}" stroke="var(--text-secondary)" stroke-dasharray="5 4"/>`+
    `<text x="${ml+iw-4}" y="${Y(o.ref.v)-6}" text-anchor="end">${o.ref.label}</text>`;
  const ends=[];
  for(const se of o.series){let d='',pen=false,single=[];
    se.v.forEach((v,i)=>{if(v==null){pen=false;return}
      const prevNull=i==0||se.v[i-1]==null,nextNull=i==n-1||se.v[i+1]==null;if(prevNull&&nextNull)single.push(i);
      d+=(pen?'L':'M')+X(i).toFixed(1)+','+Y(v).toFixed(1);pen=true});
    s+=`<path d="${d}" fill="none" stroke="${se.color}" stroke-width="2" stroke-linejoin="round" stroke-linecap="round" ${se.dash?'stroke-dasharray="6 4"':''}/>`;
    for(const i of single)s+=`<circle cx="${X(i)}" cy="${Y(se.v[i])}" r="3" fill="${se.color}"/>`;
    let li=-1;se.v.forEach((v,i)=>{if(v!=null)li=i});
    if(o.endLabels&&li>=0)ends.push({y:Y(se.v[li]),name:se.short||se.name,color:se.color})}
  ends.sort((a,b)=>a.y-b.y);for(let i=1;i<ends.length;i++)if(ends[i].y-ends[i-1].y<15)ends[i].y=ends[i-1].y+15;
  for(const e of ends)s+=`<line x1="${ml+iw+6}" x2="${ml+iw+16}" y1="${e.y}" y2="${e.y}" stroke="${e.color}" stroke-width="2"/><text x="${ml+iw+20}" y="${e.y+4}">${e.name}</text>`;
  s+=`<line class="xh" x1="0" x2="0" y1="${mt}" y2="${mt+ih}" stroke="var(--text-secondary)" stroke-width="1" visibility="hidden"/>`;
  for(const se of o.series)s+=`<circle class="dt" r="4" fill="${se.color}" stroke="var(--surface-1)" stroke-width="2" visibility="hidden"/>`;
  s+=`<rect x="${ml}" y="${mt}" width="${iw}" height="${ih}" fill="transparent"/></svg>`;
  el.innerHTML=s;const tip=evEl('div','ev-tip');el.append(tip);
  const svg=el.querySelector('svg'),xh=svg.querySelector('.xh'),dots=[...svg.querySelectorAll('.dt')];
  svg.onpointermove=e=>{const r=svg.getBoundingClientRect(),sx=(e.clientX-r.left)*W/r.width;
    const i=Math.max(0,Math.min(n-1,Math.round((sx-ml)/iw*(n-1)))),x=X(i);
    xh.setAttribute('x1',x);xh.setAttribute('x2',x);xh.setAttribute('visibility','visible');
    const rows=[evEl('div','h',o.fmtTip(o.x[i]))];
    o.series.forEach((se,k)=>{const v=se.v[i];
      if(v==null){dots[k].setAttribute('visibility','hidden');rows.push(evTipRow(se.color,se.name,'—'));return}
      dots[k].setAttribute('cx',x);dots[k].setAttribute('cy',Y(v));dots[k].setAttribute('visibility','visible');
      rows.push(evTipRow(se.color,se.name,v.toFixed(1)+o.unit))});
    tip.replaceChildren(...rows);evPlaceTip(tip,svg,x,W)};
  svg.onpointerleave=()=>{tip.style.display='none';xh.setAttribute('visibility','hidden');dots.forEach(d=>d.setAttribute('visibility','hidden'))};
}

function evBars(el,o){
  const W=960,H=270,ml=48,mr=16,mt=26,mb=26,iw=W-ml-mr,ih=H-mt-mb,n=o.x.length,ns=o.series.length;
  const mx=Math.max(0,...o.series.flatMap(s=>s.v).filter(v=>v!=null));
  if(mx<=0){evEmpty(el,'No energy data for this period yet');return}
  const tk=evTicks(0,mx,5),hi=tk[tk.length-1],Y=v=>mt+ih-v/hi*ih;
  const gw=iw/n,bw=Math.max(3,Math.min(26,(gw*0.72-2*(ns-1))/ns));
  let s=`<svg viewBox="0 0 ${W} ${H}" role="img" aria-label="${o.aria}">`;
  for(const v of tk)s+=`<line x1="${ml}" x2="${ml+iw}" y1="${Y(v)}" y2="${Y(v)}" stroke="var(--grid)"/><text x="${ml-8}" y="${Y(v)+4}" text-anchor="end">${v}</text>`;
  const every=Math.ceil(n/12);
  for(let i=0;i<n;i++){const x0=ml+i*gw+(gw-(ns*bw+2*(ns-1)))/2;
    o.series.forEach((se,j)=>{const v=se.v[i];if(!(v>0))return;const x=x0+j*(bw+2),y=Y(v),y0=Y(0),r=Math.min(4,bw/2,y0-y);
      s+=`<path d="M${x},${y0}V${y+r}Q${x},${y} ${x+r},${y}H${x+bw-r}Q${x+bw},${y} ${x+bw},${y+r}V${y0}Z" fill="${se.color}"/>`});
    if(i%every==0)s+=`<text x="${ml+i*gw+gw/2}" y="${H-6}" text-anchor="middle">${o.fmtX(o.x[i])}</text>`;
    s+=`<rect class="hit" data-i="${i}" x="${ml+i*gw}" y="${mt}" width="${gw}" height="${ih}" fill="transparent"/>`}
  s+=`<line x1="${ml}" x2="${ml+iw}" y1="${Y(0)}" y2="${Y(0)}" stroke="var(--text-secondary)"/>`;
  s+=`<text x="${ml-8}" y="12" text-anchor="end">kWh</text></svg>`;
  el.innerHTML=s;const tip=evEl('div','ev-tip');el.append(tip);const svg=el.querySelector('svg');
  svg.querySelectorAll('.hit').forEach(h=>{h.onpointerenter=()=>{const i=+h.dataset.i;h.setAttribute('fill','rgba(0,0,0,.04)');
    tip.replaceChildren(evEl('div','h',o.fmtTip(o.x[i])),...o.series.map(se=>evTipRow(se.color,se.name,se.v[i]==null?'no data':se.v[i].toFixed(2)+' kWh',true)),...(o.extra?o.extra(i):[]));
    evPlaceTip(tip,svg,ml+i*gw+gw,W)};
    h.onpointerleave=()=>{h.setAttribute('fill','transparent');tip.style.display='none'}});
}
function evTile(label,val,unit,sub,color){const t=evEl('div','ev-tile'),l=evEl('div','lbl');
  if(color){const k=evEl('span','ev-key');k.style.background=color;l.append(k)}
  l.append(document.createTextNode(label));const v=evEl('div','v',val+' ');v.append(evEl('small',null,unit));
  t.append(l,v,evEl('div','s',sub));return t}

async function loadEnergy(){
  const d=await get('/api/history',{range:EV.range});
  if(!d.temp){$('evMsg').style.display='block';$('evMsg').textContent='History not available: '+(d.err||'error')+
    '. Data is recorded only while the controller knows the time (NTP) and reads the unit.';EV.data=null;renderEnergy();loadStorage();return}
  $('evMsg').style.display='none';EV.data=d;renderEnergy();loadStorage()}
async function loadStorage(){const s=await get('/api/storage');if(!s.ok){$('evStore').textContent='';return}
  const kb=b=>b<1024*1024?Math.round(b/1024)+' KB':(b/1048576).toFixed(2)+' MB';
  const hist=s.min.bytes+s.hour.bytes+s.day.bytes;
  $('evStore').textContent=`History ${kb(hist)} · flash ${kb(s.used)} of ${kb(s.total)} used`}
async function clearHistory(){
  if(!confirm('Delete all temperature and energy history on the controller? This cannot be undone.'))return;
  if(done(await call('/api/history/clear'),'History cleared'))loadEnergy()}

function renderEnergy(){
  const d=EV.data,r=EV.range,F=EV_FMT[r],hp=+$('evHeatP').value,ep=+$('evElP').value;
  const rangeTxt={'24h':'last 24 h','7d':'last 7 days','30d':'last 30 days','12m':'last 12 months'}[r];
  if(!d){$('evTiles').replaceChildren();['evTemp','evEn','evEff'].forEach(id=>evEmpty($(id),'No data'));return}
  // temp rows: [t, room = RA, out = FR (outdoor intake), exh = OA (exhaust), sup = SA], °C × 10
  const T=d.temp.map(x=>({t:x[0]*1000,ra:x[1]==null?null:x[1]/10,fr:x[2]==null?null:x[2]/10,oa:x[3]==null?null:x[3]/10,sa:x[4]==null?null:x[4]/10}));
  const B=d.bars.map(x=>({t:x[0]*1000,kept:x[1]==null?null:x[1]/1000,lost:x[2]==null?null:x[2]/1000,fan:x[3]==null?null:x[3]/1000,out:x[4]==null?null:x[4]/10}));
  B.forEach(b=>b.eff=b.kept>0.005?100*b.kept/(b.kept+b.lost):null);
  const tot=B.reduce((a,b)=>({kept:a.kept+(b.kept||0),lost:a.lost+(b.lost||0),fan:a.fan+(b.fan||0)}),{kept:0,lost:0,fan:0});
  const without=tot.kept+tot.lost,pct=without?Math.round(100*tot.kept/without):0;
  const f1=v=>v<10?v.toFixed(2):v.toFixed(1);
  $('evTiles').replaceChildren(
    evTile('Heat kept at home',f1(tot.kept),'kWh',without?`${pct} % of ${f1(without)} kWh you would lose without the HRV`:'no heating in this period',C1),
    evTile('Heat lost',f1(tot.lost),'kWh','not recovered; heating has to cover it',C2),
    evTile('Fan electricity',f1(tot.fan),'kWh',`≈ ${(tot.fan*ep).toFixed(0)} ₴`),
    evTile('Net saving',(tot.kept*hp-tot.fan*ep).toFixed(0),'₴',rangeTxt),
    evTile('Return on fan power',tot.fan>0.001?(tot.kept/tot.fan).toFixed(1):'—','×','kWh of heat kept per kWh of fan electricity'));
  $('evTempSub').textContent=r=='24h'?'10-minute averages':r=='7d'?'hourly averages':'daily averages';
  const ts=[{name:'Return air (RA)',short:'RA',color:C1,v:T.map(x=>x.ra)},{name:'Supply air (SA)',short:'SA',color:C4,v:T.map(x=>x.sa)},
            {name:'Exhaust air (OA)',short:'OA',color:C3,v:T.map(x=>x.oa)},{name:'Fresh air (FR)',short:'FR',color:C2,v:T.map(x=>x.fr)}];
  evLegend('evTempLeg',ts);
  evLine($('evTemp'),{x:T.map(x=>x.t),series:ts,unit:'°',fmtX:F.tx,fmtTip:F.tt,endLabels:1,aria:'Temperatures, '+rangeTxt});
  const en=[{name:'Heat kept',color:C1,v:B.map(b=>b.kept)},{name:'Heat lost',color:C2,v:B.map(b=>b.lost)}];
  evLegend('evEnLeg',en.map(e=>({...e,box:1})));
  evBars($('evEn'),{x:B.map(b=>b.t),series:en,fmtX:F.bx,fmtTip:F.bt,aria:'Heat kept and lost, '+rangeTxt,
    extra:i=>B[i].fan==null?[]:[evTipRow('transparent','Fan electricity',B[i].fan.toFixed(2)+' kWh'),evTipRow('transparent','Avg outdoor (FR)',B[i].out==null?'—':B[i].out.toFixed(1)+' °C')]});
  evLine($('evEff'),{x:B.map(b=>b.t),series:[{name:'Efficiency',color:C1,v:B.map(b=>b.eff)}],unit:'%',fixedY:[0,100],h:200,
    ref:{v:75,label:'Spec 75 %'},fmtX:F.bx,fmtTip:F.bt,aria:'Heat recovery efficiency, '+rangeTxt});
  let h='<table><tr><th>Period</th><th>Kept kWh</th><th>Lost kWh</th><th>Fan kWh</th><th>Efficiency</th><th>Avg outdoor (FR)</th></tr>';
  const n2=v=>v==null?'—':v.toFixed(2);
  for(const b of B)h+=`<tr><td>${F.bt(b.t)}</td><td>${n2(b.kept)}</td><td>${n2(b.lost)}</td><td>${n2(b.fan)}</td><td>${b.eff==null?'—':b.eff.toFixed(0)+' %'}</td><td>${b.out==null?'—':b.out.toFixed(1)+' °C'}</td></tr>`;
  $('evTbl').innerHTML=h+'</table>'}
function evToggleTable(){const t=$('evTbl'),on=t.style.display=='none';t.style.display=on?'block':'none';$('evTblBtn').textContent=on?'Hide table':'Show table'}
$('evRange').onclick=e=>{const r=e.target.dataset.r;if(!r)return;EV.range=r;
  [...$('evRange').children].forEach(b=>b.classList.toggle('on',b.dataset.r==r));loadEnergy()};
function renderFlowTbl(flow,fanw){let h='<tr><th>Speed</th><th>Code</th><th>Airflow m³/h</th><th>Fans W</th></tr>';
  for(let i=1;i<=10;i++)h+=`<tr><td>${i}</td><td class="mut">${SPEED_CODES[i]}</td><td><input type="number" id="fl${i}" value="${flow[i]}"></td><td><input type="number" id="fw${i}" value="${fanw[i]}"></td></tr>`;
  $('evFlowTbl').innerHTML=h}
function flowDefaults(){renderFlowTbl(FLOW_DEF,FANW_DEF);toast('Defaults filled — press Save')}
async function saveEnergy(){const fl=[0],fw=[0];for(let i=1;i<=10;i++){fl.push(+$('fl'+i).value||0);fw.push(+$('fw'+i).value||0)}
  done(await call('/api/config',{section:'en',hp:$('evHeatP').value,ep:$('evElP').value,flow:fl.join(','),fanw:fw.join(',')}),'Energy settings saved')}
setInterval(()=>{if(TAB=='energy'&&!document.hidden)loadEnergy()},60000);
setInterval(()=>{if(TAB=='adv'&&!document.hidden)loadLog()},5000);

loadConfig().then(()=>{poll();if(location.hash)show(location.hash.slice(1))});
</script>
</body></html>
)HTMLPAGE";
