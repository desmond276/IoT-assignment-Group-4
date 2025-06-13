#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin Definitions
#define SensorPin   34    // Moisture sensor (ADC)
#define RedLED      18    // Dry indicator
#define WhiteLED    19    // Wet indicator
#define PumpPin     25    // Pump control (active HIGH)
#define DHTpin      14    // DHT11 data
#define ButtonPin   33    // Emergency/Reset
#define BuzzerPin   26    // Buzzer output

// Calibration
#define DRY_VALUE   3500
#define WET_VALUE   1200

// DHT setup
#define DHTTYPE     DHT11
DHT dht(DHTpin, DHTTYPE);

// State
int sensorVal;
int moisturePercent;
float temperature, humidity;
bool pumpRunning   = false;
bool emergencyStop = false;

// Button logic
unsigned long lastPressTime = 0;
int pressCount             = 0;

void setup() {
  Serial.begin(115200);

  pinMode(RedLED,    OUTPUT);
  pinMode(WhiteLED,  OUTPUT);
  pinMode(PumpPin,   OUTPUT);
  pinMode(ButtonPin, INPUT_PULLUP);
  pinMode(BuzzerPin, OUTPUT);

  dht.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed"));
    while (1);
  }

  // Splash
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Soil Moisture Monitor");
  display.display();
  delay(1500);
}

void readButton() {
  static bool lastState = HIGH;
  bool curr = digitalRead(ButtonPin);
  if (lastState == HIGH && curr == LOW) {
    unsigned long now = millis();
    if (now - lastPressTime < 500) pressCount++;
    else                            pressCount = 1;
    lastPressTime = now;
  }
  if (pressCount == 1 && millis() - lastPressTime > 800) {
    emergencyStop = true;  pressCount = 0;
  }
  else if (pressCount == 2 && millis() - lastPressTime > 800) {
    emergencyStop = false; pressCount = 0;
  }
  lastState = curr;
}

// Play one cycle of double beep
void playWetBeepCycle() {
  tone(BuzzerPin, 2000, 150);
  delay(200);
  tone(BuzzerPin, 2000, 150);
  delay(200);
}

void loop() {
  readButton();

  // Read sensors
  sensorVal = analogRead(SensorPin);
  sensorVal = constrain(sensorVal, WET_VALUE, DRY_VALUE);
  moisturePercent = map(sensorVal, DRY_VALUE, WET_VALUE, 0, 100);
  humidity    = dht.readHumidity();
  temperature = dht.readTemperature();

  // Emergency override
  if (emergencyStop) {
    digitalWrite(PumpPin, LOW);
    noTone(BuzzerPin);
    digitalWrite(RedLED, LOW);
    digitalWrite(WhiteLED, LOW);
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("!! EMERGENCY STOP !!");
    display.display();
    delay(500);
    return;
  }

  // Hysteresis pump control
  if (!pumpRunning && moisturePercent < 30) {
    pumpRunning = true;
  }
  else if (pumpRunning && moisturePercent >= 70) {
    pumpRunning = false;
  }
  digitalWrite(PumpPin, pumpRunning ? HIGH : LOW);

  // Buzzer logic
  if (moisturePercent < 30) {
    // Continuous dry tone
    tone(BuzzerPin, 1000);
  }
  else if (moisturePercent >= 70) {
    // Continuous double-beep cycle
    playWetBeepCycle();
  }
  else {
    noTone(BuzzerPin);
  }

  // LEDs & OLED
  display.clearDisplay();
  display.setCursor(0,0);
  display.printf("Moisture: %d%%\n", moisturePercent);
  display.printf("Temp: %.1f°C\n", temperature);
  display.printf("Hum:  %.1f%%\n\n", humidity);

  if (moisturePercent < 30) {
    digitalWrite(RedLED, HIGH);
    digitalWrite(WhiteLED, LOW);
    display.println("Status: DRY");
  }
  else if (moisturePercent >= 70) {
    digitalWrite(RedLED, LOW);
    digitalWrite(WhiteLED, HIGH);
    display.println("Status: WET");
  }
  else {
    digitalWrite(RedLED, LOW);
    digitalWrite(WhiteLED, LOW);
    display.println("Status: NORMAL");
  }

  display.printf("Pump: %s\n", pumpRunning ? "ON" : "OFF");
  display.display();

  delay(1000);
}
