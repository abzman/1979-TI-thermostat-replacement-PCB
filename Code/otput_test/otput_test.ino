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
  digitalWrite(outEnable, HIGH);   // turn the LED on (HIGH is the voltage level)
  digitalWrite(heatCool, heat);    // turn the LED off by making the voltage LOW
  delay(1000);               // wait for a second
  digitalWrite(outEnable, LOW);   // turn the LED on (HIGH is the voltage level)
  delay(1000);               // wait for a second
  digitalWrite(outEnable, HIGH);   // turn the LED on (HIGH is the voltage level)
  digitalWrite(heatCool, cool);    // turn the LED off by making the voltage LOW
  delay(1000);               // wait for a second
  digitalWrite(outEnable, LOW);   // turn the LED on (HIGH is the voltage level)
  delay(1000);               // wait for a second
  digitalWrite(fan, LOW);// fan
  delay(1000);               // wait for a second
  digitalWrite(fan, HIGH);//no fan
  delay(1000);               // wait for a second
  digitalWrite(hunid, LOW);// humid
  delay(1000);               // wait for a second
  digitalWrite(hunid, HIGH);//no humid
  delay(1000);               // wait for a second
}
