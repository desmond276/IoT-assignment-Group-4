#define SensorPin A0  // Moisture sensor pin (analog)
#define LED_PIN LED_BUILTIN

int sensorVal;
int threshold = 500;  // Default threshold value

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("=== Smart Plant Monitoring System ===");
  Serial.println("Enter moisture threshold (e.g. 450): ");
}

void loop() {
  
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    threshold = input.toInt();
    Serial.print("Threshold updated to: ");
    Serial.println(threshold);
  }

  // Read moisture sensor
  sensorVal = analogRead(SensorPin);
  Serial.print("Moisture Value: ");
  Serial.println(sensorVal);

  // LED feedback based on threshold
  if (sensorVal < threshold) {
    Serial.println("[ALERT] Soil is dry!");
    digitalWrite(LED_PIN, LOW);  // LED ON
  } else {
    digitalWrite(LED_PIN, HIGH); // LED OFF
  }

  delay(1000);
}
