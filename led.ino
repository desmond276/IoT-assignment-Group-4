#define SensorPin A0       // Analog pin A0 for moisture sensor
#define RedLED    D6       // Red LED pin
#define WhiteLED  D5       // White LED pin

int sensorVal;

void setup() {
  Serial.begin(115200);
  pinMode(RedLED, OUTPUT);
  pinMode(WhiteLED, OUTPUT);
}

void loop() {
  sensorVal = analogRead(SensorPin);
  Serial.print("Moisture Value: ");
  Serial.println(sensorVal);

  if (sensorVal < 600) {
    // Dry moisture - turn on Red LED
    digitalWrite(RedLED, LOW);
    digitalWrite(WhiteLED, HIGH);
  }
  else if (sensorVal > 300) {
    // Wet moisture - turn on White LED
    digitalWrite(RedLED, HIGH);
    digitalWrite(WhiteLED, LOW);
  }
  else {
    // Normal range - turn off both LEDs
    digitalWrite(RedLED, LOW);
    digitalWrite(WhiteLED, LOW);
  }

  delay(1000); // Wait for 1 second
}