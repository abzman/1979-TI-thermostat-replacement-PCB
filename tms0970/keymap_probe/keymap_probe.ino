// keymap_probe.ino - maps the thermostat keypad onto the 74HC138 address / data pins.
// Board: ESP32 (WROVER). Serial Monitor: 115200 baud, "Newline" line ending.
//
// It asks you to press keys one at a time, waits until a key is clearly pressed (same single
// contact seen on many consecutive scans), then waits for release and asks for the next one.
// At the end it prints a report between two marker lines - copy that block back to Claude.
// Type 's' + Enter at any prompt to skip a key that doesn't respond.

const uint8_t ADDR_PIN[3] = {25, 33, 32};        // 74HC138 A0, A1, A2
const uint8_t DATA_PIN[4] = {36, 39, 34, 35};    // data inputs (external pull-ups; 34-39 have none inside)
const int KEY_ADDRS = 7;                         // scan addresses 0..6; address 7 is the switch line, ignored here
const int SETTLE_US = 500;                       // let the 138 + pull-ups settle after changing address
const int STABLE_SCANS = 20;                     // full scans a reading must persist (~70 ms) = "debounced"

// Keys to ask for, in this order. (The last two are extras that complete the row map.)
const char *KEYS[] = {"0", "5", "9", "1", "Time/Clock", "Temp/Actual", "8", "7"};
const int NKEYS = sizeof(KEYS) / sizeof(KEYS[0]);

uint8_t idleRow[8];                              // data bits per address with nothing pressed (1 = high)
int foundAddr[NKEYS], foundBit[NKEYS], foundLevel[NKEYS];   // addr -1 = skipped

uint8_t readRow(uint8_t a) {
  for (int i = 0; i < 3; i++) digitalWrite(ADDR_PIN[i], (a >> i) & 1);
  delayMicroseconds(SETTLE_US);
  uint8_t v = 0;
  for (int b = 0; b < 4; b++) if (digitalRead(DATA_PIN[b])) v |= 1 << b;
  return v;
}

// One full scan of addresses 0..6. Returns how many (address, bit) contacts differ from idle.
// If exactly one, its address/bit/level are returned.
int scanActive(int &addr, int &bit, int &level) {
  int n = 0;
  for (int a = 0; a < KEY_ADDRS; a++) {
    uint8_t diff = readRow(a) ^ idleRow[a];
    for (int b = 0; b < 4; b++)
      if (diff & (1 << b)) { n++; addr = a; bit = b; level = (idleRow[a] >> b & 1) ? 0 : 1; }
  }
  return n;
}

bool skipRequested() {
  bool s = false;
  while (Serial.available()) { int c = Serial.read(); if (c == 's' || c == 'S') s = true; }
  return s;
}

void waitRelease() {
  int ok = 0, a, b, l;
  while (ok < STABLE_SCANS) { ok = (scanActive(a, b, l) == 0) ? ok + 1 : 0; }
}

// Returns 1 = got a stable single key, 0 = skipped, -1 = several contacts at once
int waitKey(int &addr, int &bit, int &level) {
  int stable = 0, lastA = -1, lastB = -1, a = 0, b = 0, l = 0;
  int multi = 0;
  while (true) {
    if (skipRequested()) return 0;
    int n = scanActive(a, b, l);
    if (n == 1) {
      multi = 0;
      if (a == lastA && b == lastB) stable++; else { lastA = a; lastB = b; stable = 1; }
      if (stable >= STABLE_SCANS) { addr = a; bit = b; level = l; return 1; }
    } else if (n > 1) {
      stable = 0; lastA = lastB = -1;
      if (++multi >= STABLE_SCANS) return -1;
    } else {
      stable = 0; lastA = lastB = -1; multi = 0;
    }
  }
}

void captureIdle() {
  Serial.println("Don't touch any keys... measuring idle state.");
  delay(1500);
  while (true) {
    uint8_t first[8]; bool same = true;
    for (int a = 0; a < 8; a++) first[a] = readRow(a);
    for (int rep = 0; rep < 50 && same; rep++)
      for (int a = 0; a < 8; a++) if (readRow(a) != first[a]) same = false;
    if (same) { for (int a = 0; a < 8; a++) idleRow[a] = first[a]; return; }
    Serial.println("idle state is not steady - hands off the keys (retrying)");
    delay(500);
  }
}

void printReport() {
  Serial.println();
  Serial.println("=== KEYMAP REPORT (copy from this line) ===");
  Serial.println("data index: 0=IO36 1=IO39 2=IO34 3=IO35");
  Serial.print("idle bits per address (data idx3..idx0, 1=high):");
  for (int a = 0; a < 8; a++) {
    Serial.print(" ");
    Serial.print(a);
    Serial.print(":");
    for (int b = 3; b >= 0; b--) Serial.print((idleRow[a] >> b) & 1);
  }
  Serial.println();
  for (int k = 0; k < NKEYS; k++) {
    Serial.print("key '"); Serial.print(KEYS[k]); Serial.print("': ");
    if (foundAddr[k] < 0) { Serial.println("SKIPPED"); continue; }
    Serial.print("addr "); Serial.print(foundAddr[k]);
    Serial.print(", data idx "); Serial.print(foundBit[k]);
    Serial.print(" (IO"); Serial.print(DATA_PIN[foundBit[k]]);
    Serial.print("), pressed level "); Serial.println(foundLevel[k] ? "HIGH" : "LOW");
  }
  Serial.println("=== END OF REPORT ===");
  Serial.println("(type 'r' + Enter to print the report again)");
}

void runProbe() {
  captureIdle();
  for (int k = 0; k < NKEYS; k++) {
    foundAddr[k] = -1;
    while (true) {
      Serial.print("Press and hold key '"); Serial.print(KEYS[k]);
      Serial.println("'  (or type s + Enter to skip)");
      int a = 0, b = 0, l = 0;
      int r = waitKey(a, b, l);
      if (r == 0) { Serial.println("  skipped"); break; }
      if (r < 0) { Serial.println("  more than one contact at once - release everything and try again"); waitRelease(); continue; }
      int dup = -1;
      for (int j = 0; j < k; j++) if (foundAddr[j] == a && foundBit[j] == b) dup = j;
      if (dup >= 0) {
        Serial.print("  that's the same contact as key '"); Serial.print(KEYS[dup]);
        Serial.println("' - release and press the right key");
        waitRelease(); continue;
      }
      foundAddr[k] = a; foundBit[k] = b; foundLevel[k] = l;
      Serial.print("  got it: addr "); Serial.print(a); Serial.print(", data IO"); Serial.print(DATA_PIN[b]);
      Serial.println("  - release the key");
      waitRelease();
      delay(150);
      break;
    }
  }
  printReport();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  for (int i = 0; i < 3; i++) { pinMode(ADDR_PIN[i], OUTPUT); digitalWrite(ADDR_PIN[i], 0); }
  for (int i = 0; i < 4; i++) pinMode(DATA_PIN[i], INPUT);
  Serial.println();
  Serial.println("Thermostat keymap probe");
  runProbe();
}

void loop() {
  if (Serial.available()) { int c = Serial.read(); if (c == 'r' || c == 'R') printReport(); }
}
