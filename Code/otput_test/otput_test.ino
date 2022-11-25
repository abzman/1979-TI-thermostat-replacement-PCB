#define outEnable 4
#define hunid 27
#define fan 2
#define heatCool 19
#define heat 1
#define cool 0

// the setup routine runs once when you press reset:
void setup() {
  pinMode(outEnable, OUTPUT);
  pinMode(hunid, OUTPUT);
  pinMode(fan, OUTPUT);
  pinMode(heatCool, OUTPUT);
  
  digitalWrite(outEnable, LOW);//off
  digitalWrite(hunid, HIGH);//no humid
  digitalWrite(fan, HIGH);//no fan
  digitalWrite(heatCool, heat);
}

// the loop routine runs over and over again forever:
void loop() {
  digitalWrite(fan, LOW);// fan
  delay(30000);               // wait for a second
  digitalWrite(fan, HIGH);//no fan
  delay(30000);               // wait for a second
}
