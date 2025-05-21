#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// OLED Display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin Definitions
#define SensorPin   34   // Moisture sensor (ADC)
#define RedLED      18   // Red LED (Dry)
#define WhiteLED    19   // White LED (Wet)
#define PumpPin     25   // Pump transistor base (active HIGH)
#define DHTpin      14   // DHT11 sensor
#define ButtonPin   33   // Emergency/Reset button

// Calibration values
#define DRY_VALUE   3500
#define WET_VALUE   1200

// DHT setup
#define DHTTYPE     DHT11
DHT dht(DHTpin, DHTTYPE);

// State variables
int sensorVal;
int moisturePercent;
float temperature, humidity;
bool pumpRunning    = false;
bool emergencyStop  = false;
unsigned long lastPressTime = 0;
int pressCount      = 0;

void setup() {
  Serial.begin(115200);

  pinMode(RedLED, OUTPUT);
  pinMode(WhiteLED, OUTPUT);
  pinMode(PumpPin, OUTPUT);
  pinMode(ButtonPin, INPUT_PULLUP);

  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED not found"));
    while (1);
  }

  // Intro splash
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
  bool currState = digitalRead(ButtonPin);

  if (lastState == HIGH && currState == LOW) {
    unsigned long now = millis();
    if (now - lastPressTime < 500) {
      pressCount++;
    } else {
      pressCount = 1;
    }
    lastPressTime = now;
  }

  // Single press → emergency stop
  if (pressCount == 1 && millis() - lastPressTime > 800) {
    emergencyStop = true;
    pressCount = 0;
  }
  // Double press → reset
  else if (pressCount == 2 && millis() - lastPressTime > 800) {
    emergencyStop = false;
    pressCount = 0;
  }

  lastState = currState;
}

void loop() {
  readButton();

  // 1) Read sensors
  sensorVal = analogRead(SensorPin);
  sensorVal = constrain(sensorVal, WET_VALUE, DRY_VALUE);
  moisturePercent = map(sensorVal, DRY_VALUE, WET_VALUE, 0, 100);
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  // 2) Emergency override
  if (emergencyStop) {
    digitalWrite(PumpPin, LOW);        // pump OFF
    digitalWrite(RedLED, LOW);
    digitalWrite(WhiteLED, LOW);

    display.clearDisplay();
    display.setCursor(0,0);
    display.println("!! EMERGENCY STOP !!");
    display.display();
    delay(500);
    return;
  }

  // 3) Hysteresis pump control
  if (!pumpRunning && moisturePercent < 30) {
    pumpRunning = true;   // start pump
  }
  else if (pumpRunning && moisturePercent >= 70) {
    pumpRunning = false;  // stop pump
  }
  digitalWrite(PumpPin, pumpRunning ? HIGH : LOW);

  // 4) LEDs & OLED status
  display.clearDisplay();
  display.setCursor(0,0);
  display.printf("Moisture: %d%%\n", moisturePercent);
  display.printf("Temp: %.1f C\n", temperature);
  display.printf("Hum: %.1f %%\n\n", humidity);

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
