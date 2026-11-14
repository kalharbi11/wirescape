// SPDX-License-Identifier: GPL-3.0-only
/*
  WirescapeOSC: turn pots and buttons into OSC messages for Wirescape.
  ======================================================================

  Boards: written for ESP32-S3. Also builds for the other ESP32 chips
  (ESP32, S2, C3, C6), ESP8266, and Raspberry Pi Pico W / Pico 2 W
  (arduino-pico core). Only the CONTROLS table and WiFi settings change.

  No extra libraries: only the board's own WiFi + UDP.

  WHAT IT SENDS
  -------------
  Every control sends one OSC message to Wirescape over WiFi (UDP):

      /pico1/knob/1   <float 0..1>   <int raw ADC value>
      /pico1/button/1 <float 0 or 1> <int 0 or 1>

  - Argument 0 (float) is already scaled to 0..1 using the control's
    rawMin/rawMax below, so Wirescape's default OSC In rows work as is.
  - Argument 1 (int) is the untouched raw reading. Use it if you want
    Wirescape to do the scaling instead: set the row's Argument to 1,
    then use Set max / Set min (or type Raw min / Raw max) in the editor.
  - The prefix "pico1" matches Wirescape's default rows
    (/pico1/knob/1 ... /pico1/knob/8), so knobs 1-8 need no editor change.
    Using a second board? Give it DEVICE "pico2" and point rows at it.

  Messages are sent when a value changes, plus every HEARTBEAT_MS for
  every control, so Wirescape never marks a resting knob as "stale".

  WIRESCAPE SIDE (once)
  ---------------------
  1. Patch an OSC In module (port 9000, matches WIRESCAPE_PORT below).
  2. Edit... -> tick LAN. A board on WiFi is a LAN device, and Wirescape
     only listens to the network when the app was ALSO started with
     --confirm-lan. Without both, it only listens to this computer.
  3. Patch OSC In "messages" -> a Monitor "events" to see every message
     (address + both arguments) as you turn the knobs.
  4. Patch OSC In row1..row8 to whatever you want to control.
  Buttons: set a spare row's address to /pico1/button/1 and Kind to Gate.

  CALIBRATING A POT (any pot, any range)
  --------------------------------------
  Open Tools -> Serial Monitor at 115200. It prints each control's raw
  value. Turn a knob fully left, note the number; fully right, note it.
  Put those in that knob's rawMin / rawMax below and upload again.
  (Or leave the defaults and calibrate in Wirescape with argument 1.)

  WIRING (ESP32-S3)
  -----------------
  Pot (10k linear is ideal, 1k-100k works):
      one outer leg -> 3V3     (NEVER 5V: ESP32 pins are 3.3 V max)
      other outer leg -> GND
      middle leg (wiper) -> the GPIO in the table
  Button: one side -> the GPIO, other side -> GND (internal pull-up used).

  ESP32-S3 PIN RULES
  - Pots MUST use ADC1 pins: GPIO 1-10. ADC2 pins (GPIO 11-20) stop
    working while WiFi is on.
  - Avoid GPIO 0, 3, 45, 46 (boot/strapping pins), 19/20 (USB),
    and 35-37 on boards with octal PSRAM (most N8R8/N16R8 modules).
  - Buttons can use almost any other free GPIO, e.g. 15, 16, 17, 18, 21.

  Other boards: classic ESP32 pots on ADC1 = GPIO 32-39 (36/39 are
  input-only, fine for pots). ESP32-C3: GPIO 0-4. Pico W: GP26-28
  (A0-A2). ESP8266: only one analog pin, A0, and its range is 0-1 V on a
  bare chip (most dev boards divide 3.3 V down for you).
*/

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>            // ESP32 family, Pico W / Pico 2 W
#endif
#include <WiFiUdp.h>

// ============================ 1. YOUR WIFI ============================
const char *WIFI_SSID     = "your-wifi-name";
const char *WIFI_PASSWORD = "your-wifi-password";

// ========================= 2. WHERE WIRESCAPE IS ======================
// The IP address of the computer running Wirescape (Windows: `ipconfig`,
// look for IPv4 Address). Leave "" to broadcast to the whole local
// network instead, which works without knowing the IP but some routers
// or guest networks block it.
const char    *WIRESCAPE_IP   = "192.168.1.50";
const uint16_t WIRESCAPE_PORT = 9000;      // = OSC In port in Wirescape
const char    *DEVICE         = "pico1";   // first part of every address

// ============================ 3. CONTROLS ============================
// One line per pot or button. Add, remove or change lines freely.
//
//   kind     KNOB (pot / slider / any analog sensor) or BUTTON
//   pin      GPIO number (see PIN RULES above)
//   number   address number: KNOB 1 -> /pico1/knob/1 -> Wirescape row1
//   rawMin   raw value at the knob's "zero" end   (KNOB only)
//   rawMax   raw value at the knob's "full" end   (KNOB only)
//            Defaults 0 / ADC_MAX cover the whole pin range. A pot that
//            reads e.g. 121..1689 -> put 121 and 1689 here.
//            Swap them (rawMin > rawMax) to reverse the knob.
enum Kind { KNOB, BUTTON };
struct Control { Kind kind; uint8_t pin; uint8_t number; int rawMin; int rawMax; };

#if defined(ESP8266)
  const int ADC_MAX = 1023;              // 10-bit ADC
#else
  const int ADC_MAX = 4095;              // 12-bit ADC
#endif

Control CONTROLS[] = {
  // kind    pin  number  rawMin  rawMax          ESP32-S3 breadboard
  { KNOB,     4,    1,      0,    ADC_MAX },   // pot wiper -> GPIO 4
  { KNOB,     5,    2,      0,    ADC_MAX },   // pot wiper -> GPIO 5
  { KNOB,     6,    3,      0,    ADC_MAX },   // pot wiper -> GPIO 6
  { BUTTON,  15,    1,      0,    0       },   // button    -> GPIO 15 + GND
  // { KNOB,  7,    4,      0,    ADC_MAX },   // a 4th pot -> GPIO 7
};

// ============================ 4. TUNING ===============================
// Defaults suit most pots; change only if you need to.
const uint8_t  OVERSAMPLE     = 8;    // ADC reads averaged per sample
const float    SMOOTHING      = 0.25; // 0..1, lower = smoother but slower
const int      DEADBAND       = 6;    // raw steps of change needed to send
                                      // (raise it if values flicker at rest)
const uint32_t SEND_EVERY_MS  = 5;    // check controls ~200 times a second
const uint32_t HEARTBEAT_MS   = 400;  // resend everything this often
                                      // (keep under Wirescape's 1000 ms timeout)
const uint32_t DEBOUNCE_MS    = 15;   // button bounce filter
const bool     SERIAL_PRINT   = true; // print raw values for calibration

// ======================= NOTHING TO EDIT BELOW ========================

const size_t COUNT = sizeof(CONTROLS) / sizeof(CONTROLS[0]);
struct State {
  float filtered = -1;   // smoothed raw value (-1 = not read yet)
  int sentRaw = -100000; // last raw value sent
  int stable = 0;        // debounced button state (1 = pressed)
  int reading = 0;       // last raw button reading
  uint32_t changedAt = 0;
  char address[48];
};
State states[COUNT];

WiFiUDP udp;
IPAddress target;
uint32_t lastSample = 0, lastBeat = 0, lastPrint = 0, lastWifiTry = 0;

// ---- OSC 1.0 encoding: address, ",fi", float32, int32 (big-endian) ----
static size_t padded(size_t n) { return (n + 4) & ~size_t(3); } // incl. NUL
static void putBE(uint8_t *p, uint32_t v) {
  p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}
size_t encodeOsc(uint8_t *out, const char *address, float value, int32_t raw) {
  size_t n = 0, a = strlen(address), pa = padded(a);
  memset(out, 0, pa + 12);
  memcpy(out, address, a);        n += pa;
  memcpy(out + n, ",fi", 3);      n += 4;
  uint32_t bits; memcpy(&bits, &value, 4);
  putBE(out + n, bits);           n += 4;
  putBE(out + n, uint32_t(raw));  n += 4;
  return n;
}

void sendControl(size_t i, float value, int raw) {
  if (WiFi.status() != WL_CONNECTED) return;
  uint8_t packet[64];
  size_t n = encodeOsc(packet, states[i].address, value, raw);
  udp.beginPacket(target, WIRESCAPE_PORT);
  udp.write(packet, n);
  udp.endPacket();
}

int readRaw(uint8_t pin) {
  long sum = 0;
  for (uint8_t k = 0; k < OVERSAMPLE; ++k) sum += analogRead(pin);
  return int(sum / OVERSAMPLE);
}

float scaled(const Control &c, int raw) {
  if (c.rawMax == c.rawMin) return 0;
  float v = float(raw - c.rawMin) / float(c.rawMax - c.rawMin); // swap = reverse
  return v < 0 ? 0 : (v > 1 ? 1 : v);
}

void chooseTarget() {
  if (WIRESCAPE_IP[0] && target.fromString(WIRESCAPE_IP)) return;
  IPAddress ip = WiFi.localIP(), mask = WiFi.subnetMask();
  for (int k = 0; k < 4; ++k) target[k] = ip[k] | (~mask[k] & 0xFF); // broadcast
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (lastWifiTry && millis() - lastWifiTry < 15000) return; // give it time
  lastWifiTry = millis();
  Serial.printf("WiFi: connecting to %s ...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void setup() {
  Serial.begin(115200);
  delay(300);
#if !defined(ESP8266)
  analogReadResolution(12);
#endif
  for (size_t i = 0; i < COUNT; ++i) {
    const Control &c = CONTROLS[i];
    snprintf(states[i].address, sizeof(states[i].address), "/%s/%s/%u",
             DEVICE, c.kind == KNOB ? "knob" : "button", c.number);
    if (c.kind == BUTTON) pinMode(c.pin, INPUT_PULLUP);
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
#if defined(ARDUINO_ARCH_ESP32)
  WiFi.setSleep(false);                // WiFi power save adds lag/jitter
#endif
  connectWifi();
  udp.begin(9001);
}

void loop() {
  static bool online = false;
  connectWifi();
  if (WiFi.status() == WL_CONNECTED && !online) {
    online = true;
    chooseTarget();
    Serial.print("WiFi: connected, this board is ");
    Serial.print(WiFi.localIP());
    Serial.print(", sending OSC to ");
    Serial.print(target);
    Serial.printf(":%u\n", WIRESCAPE_PORT);
  } else if (WiFi.status() != WL_CONNECTED) online = false;

  uint32_t now = millis();
  if (now - lastSample < SEND_EVERY_MS) return;
  lastSample = now;
  bool beat = now - lastBeat >= HEARTBEAT_MS;
  if (beat) lastBeat = now;

  for (size_t i = 0; i < COUNT; ++i) {
    const Control &c = CONTROLS[i];
    State &s = states[i];
    if (c.kind == KNOB) {
      int raw = readRaw(c.pin);
      s.filtered = s.filtered < 0 ? raw : s.filtered + SMOOTHING * (raw - s.filtered);
      int value = int(s.filtered + 0.5f);
      // Always land exactly on the ends, so 0 and 1 are reachable.
      if (value <= min(c.rawMin, c.rawMax) + DEADBAND / 2) value = min(c.rawMin, c.rawMax);
      if (value >= max(c.rawMin, c.rawMax) - DEADBAND / 2) value = max(c.rawMin, c.rawMax);
      if (abs(value - s.sentRaw) >= DEADBAND || (value != s.sentRaw &&
          (value == c.rawMin || value == c.rawMax)) || beat) {
        s.sentRaw = value;
        sendControl(i, scaled(c, value), value);
      }
    } else {
      int pressed = digitalRead(c.pin) == LOW ? 1 : 0;
      if (pressed != s.reading) { s.reading = pressed; s.changedAt = now; }
      bool changed = false;
      if (now - s.changedAt >= DEBOUNCE_MS && s.reading != s.stable) {
        s.stable = s.reading; changed = true;
      }
      if (changed || beat) sendControl(i, float(s.stable), s.stable);
    }
  }

  if (SERIAL_PRINT && now - lastPrint >= 250) {
    lastPrint = now;
    for (size_t i = 0; i < COUNT; ++i) {
      const Control &c = CONTROLS[i];
      if (c.kind == KNOB)
        Serial.printf("%s raw=%4d out=%.3f   ", states[i].address,
                      int(states[i].filtered + 0.5f), scaled(c, int(states[i].filtered + 0.5f)));
      else
        Serial.printf("%s %s   ", states[i].address, states[i].stable ? "DOWN" : "up");
    }
    Serial.println();
  }
}
