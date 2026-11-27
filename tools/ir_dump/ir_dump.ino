// IR remote scanner for the HKL-EA8 (IR receiver on GPIO33).
// Flash it, open the serial monitor at 115200, point the AC remote at the
// board and press a button. It prints the protocol, the decoded AC state
// and the raw timings. Then flash hrv_control again.
// Needs the IRremoteESP8266 library (2.9.0 compiles on ESP32 core 4.0).
#include <IRrecv.h>
#include <IRutils.h>
#include <IRac.h>

#define IR_RX_PIN 33

IRrecv rx(IR_RX_PIN, 1024, 50, true);  // big buffer and long timeout: AC frames are long
decode_results r;

void setup() {
  Serial.begin(115200);
  rx.setUnknownThreshold(12);
  rx.enableIRIn();
  Serial.println("IR dump ready, press a button on the remote");
}

void loop() {
  if (!rx.decode(&r)) return;
  Serial.println("--------------------------------");
  Serial.print(resultToHumanReadableBasic(&r));
  String ac = IRAcUtils::resultAcToString(&r);
  if (ac.length()) Serial.println("AC state: " + ac);
  Serial.println(resultToSourceCode(&r));
  rx.resume();
}
