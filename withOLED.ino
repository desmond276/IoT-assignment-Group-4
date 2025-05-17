#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// OLED Display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin Definitions
#define SensorPin 34         // Moisture sensor (ADC)
#define RedLED    18         // Red LED (Dry)
#define WhiteLED  19         // White LED (Wet)
#define PumpPin   25         // Water pump control
#define DHTpin    14         // DHT11 sensor
#define ButtonPin 33         // Emergency/Reset button (multi-press)

#define DRY_VALUE  3500      // Dry reading from sensor
#define WET_VALUE  1200      // Wet reading from sensor
#define DHTTYPE    DHT11

DHT dht(DHTpin, DHTTYPE);

// Variables
int sensorVal;
int moisturePercent;
float temperature;
float humidity;

bool pumpRunning = false;
bool emergencyStop = false;

// For button press logic
unsigned long lastPressTime = 0;
int pressCount = 0;

void setup() {
  Serial.begin(115200);

  // Pins setup
  pinMode(RedLED, OUTPUT);
  pinMode(WhiteLED, OUTPUT);
  pinMode(PumpPin, OUTPUT);
  pinMode(ButtonPin, INPUT_PULLUP); // Button with internal pull-up

  // DHT11 Start
  dht.begin();

  // OLED Start
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED not found"));
    while (true);
  }

  // Initial splash
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Soil Moisture Monitor");
  display.display();
  delay(1500);
}

void readButton() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(ButtonPin);

  if (lastButtonState == HIGH && currentButtonState == LOW) {
    unsigned long now = millis();

    if (now - lastPressTime < 500) {
      pressCount++;
    } else {
      pressCount = 1;
    }

    lastPressTime = now;
  }

  if (pressCount == 1 && millis() - lastPressTime > 800) {
    emergencyStop = true;
    pressCount = 0;
  } else if (pressCount == 2 && millis() - lastPressTime > 800) {
    emergencyStop = false;
    pressCount = 0;
  }

  lastButtonState = currentButtonState;
}

void loop() {
  readButton();

  // Read sensor
  sensorVal = analogRead(SensorPin);
  sensorVal = constrain(sensorVal, WET_VALUE, DRY_VALUE);
  moisturePercent = map(sensorVal, DRY_VALUE, WET_VALUE, 0, 100);

  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  // OLED Display
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Moisture: ");
  display.print(moisturePercent);
  display.println("%");

  display.setCursor(0, 10);
  display.print("Temp: ");
  display.print(temperature);
  display.println(" C");

  display.setCursor(0, 20);
  display.print("Humidity: ");
  display.print(humidity);
  display.println(" %");

  if (emergencyStop) {
    digitalWrite(PumpPin, HIGH); // Turn OFF pump
    digitalWrite(RedLED, LOW);
    digitalWrite(WhiteLED, LOW);
    display.setCursor(0, 40);
    display.println("EMERGENCY STOP");
  } else {
    // Hysteresis control: ON <30%, OFF >=70%
    if (!pumpRunning && moisturePercent < 30) {
      pumpRunning = true;
    } else if (pumpRunning && moisturePercent >= 70) {
      pumpRunning = false;
    }

    digitalWrite(PumpPin, pumpRunning ? LOW : HIGH); // LOW = ON (active low relay)
    display.setCursor(0, 40);
    display.print("Pump: ");
    display.println(pumpRunning ? "ON" : "OFF");

    // LED Indicators
    if (moisturePercent < 30) {
      digitalWrite(RedLED, HIGH);
      digitalWrite(WhiteLED, LOW);
      display.setCursor(0, 50);
      display.println("Status: Dry");
    } else if (moisturePercent >= 70) {
      digitalWrite(RedLED, LOW);
      digitalWrite(WhiteLED, HIGH);
      display.setCursor(0, 50);
      display.println("Status: Wet");
    } else {
      digitalWrite(RedLED, LOW);
      digitalWrite(WhiteLED, LOW);
      display.setCursor(0, 50);
      display.println("Status: Normal");
    }
  }

  display.display();
  delay(1000);
}
