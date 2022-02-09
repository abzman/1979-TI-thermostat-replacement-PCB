
int addr_pin[] = {25, 33, 32};
int address = 7; //so it starts counting at 0
#define Data0 36
#define Data1 39
#define Data2 34
#define Data3 35


// the setup routine runs once when you press reset:
void setup() {
  // initialize the digital pin as an output.
  for (int i = 0; i < sizeof(addr_pin) / sizeof(addr_pin[0]); ++i) {
    pinMode(addr_pin[i], OUTPUT);
    digitalWrite(addr_pin[i], 0);
  }
  pinMode(Data0, INPUT);
  pinMode(Data1, INPUT);
  pinMode(Data2, INPUT);
  pinMode(Data3, INPUT);
  Serial.begin(115200);
  Serial.println("begin matrix decoder");
}

// the loop routine runs over and over again forever:
void loop() {
  //set address line
  address += 1;
  if (address > 7) {
    address = 0;
    delay(1000);
    Serial.println();
  }
  for (int i = 0; i < sizeof(addr_pin) / sizeof(addr_pin[0]); ++i) {
    digitalWrite(addr_pin[i], (address >> i) % 2);
  }
  delay(1);
  Serial.print(digitalRead(Data0));
  Serial.print(digitalRead(Data1));
  Serial.print(digitalRead(Data2));
  Serial.print(digitalRead(Data3));
}
