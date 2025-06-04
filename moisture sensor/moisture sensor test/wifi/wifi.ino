#define BLYNK_TEMPLATE_ID       "TMPL6lxVBUFPN"
#define BLYNK_TEMPLATE_NAME     "desmond"
#define BLYNK_AUTH_TOKEN        "EP3vMgTn7Cg083fyI1bVEErHbEKK1Rmr"
#define BLYNK_PRINTSerial   // Debug prints

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// Wi-Fi Credentials
char ssid[] = "";
char pass[] = "";

// Virtual Pins
#define VPIN_MOISTURE V0
#define VPIN_TEMP     V1
#define VPIN_HUM      V2
#define VPIN_PUMP     V3
#define VPIN_EMER     V4

BlynkTimer timer;

// On-board LED (GPIO2) to blink once when Wi-Fi/Blynk connects
#define WIFI_LED     2

// OLED Configuration
#define OLED_WIDTH   128
#define OLED_HEIGHT  64
#define OLED_RESET   -1
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// Pin Definitions
#define SENSOR_PIN    34    // ADC1_6 (soil moisture)
#define RED_LED       18    // Dry indicator
#define WHITE_LED     19    // Wet indicator
#define PUMP_PIN      25    // Pump control (BJT, active HIGH)
#define BUZZER_PIN    26    // Buzzer output
#define BUTTON_STOP   32    // Emergency STOP button
#define BUTTON_RESET  33    // Emergency RESET button
#define DHT_PIN       14    // DHT11 data pin

// Calibration values for soil sensor
#define DRY_RAW       3500
#define WET_RAW       1200

// DHT setup
#define DHTTYPE       DHT11
DHT dht(DHT_PIN, DHTTYPE);

// State Variables
int sensorRaw;
int moisturePct;
float temperature, humidity;
bool pumpOn    = false;
bool emergency = false;

//------------------------------------------------------------------------------
// Handle single-press STOP / RESET buttons
void checkButtons() {
  static bool lastStopState  = HIGH;
  static bool lastResetState = HIGH;

  bool currStopState  = digitalRead(BUTTON_STOP);
  bool currResetState = digitalRead(BUTTON_RESET);

  // Emergency STOP: when STOP button goes HIGH→LOW
  if (lastStopState == HIGH && currStopState == LOW) {
    emergency = true;
    Serial.println(">>> EMERGENCY STOP activated <<<");
  }

  // Emergency RESET: when RESET button goes HIGH→LOW
  if (lastResetState == HIGH && currResetState == LOW) {
    emergency = false;
    Serial.println(">>> EMERGENCY RESET <<<");
  }

  lastStopState  = currStopState;
  lastResetState = currResetState;
}

//------------------------------------------------------------------------------
// Play two short beeps for “wet” alert
void wetBeep() {
  tone(BUZZER_PIN, 2000, 150);
  delay(200);
  tone(BUZZER_PIN, 2000, 150);
}

//------------------------------------------------------------------------------
// updateReading(): called by BlynkTimer every 1 second.
// – reads sensors
// – updates pump, LEDs, buzzer, OLED
// – sends data to Blynk (all virtualWrites here)
void updateReading() {
  checkButtons();

  // 1) Read raw moisture and map to 0–100%
  sensorRaw   = analogRead(SENSOR_PIN);
  sensorRaw   = constrain(sensorRaw, WET_RAW, DRY_RAW);
  moisturePct = map(sensorRaw, DRY_RAW, WET_RAW, 0, 100);

  // 2) Read DHT11
  humidity    = dht.readHumidity();
  temperature = dht.readTemperature();

  // 3) Emergency overrides pump & buzzer
  if (emergency) {
    pumpOn = false;
    noTone(BUZZER_PIN);
  }
  else {
    // 4) Hysteresis Pump Logic: ON <30%, OFF ≥70%
    if (!pumpOn && moisturePct < 30) {
      pumpOn = true;
    }
    else if (pumpOn && moisturePct >= 70) {
      pumpOn = false;
      wetBeep();
    }
  }
  digitalWrite(PUMP_PIN, pumpOn ? HIGH : LOW);

  // 5) Buzzer: continuous tone when dry, silent normal, wet beep done above
  if (!emergency) {
    if (moisturePct < 30)       tone(BUZZER_PIN, 1000);
    else if (moisturePct < 70)  noTone(BUZZER_PIN);
  }

  // 6) LEDs
  digitalWrite(RED_LED,   (moisturePct < 30) ? HIGH : LOW);
  digitalWrite(WHITE_LED, (moisturePct >= 70) ? HIGH : LOW);

  // 7) OLED Display
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.printf("Moisture: %3d%%\n", moisturePct);
  display.printf("Temp:     %.1f C\n", temperature);
  display.printf("Humidity: %.1f %%\n\n", humidity);
  display.printf("Pump: %s\n", pumpOn ? "ON" : "OFF");

  if (moisturePct < 30)       display.println("Soil: DRY");
  else if (moisturePct >= 70) display.println("Soil: WET");
  else                         display.println("Soil: NORMAL");

  if (emergency) display.println("!! EMERGENCY !!");

  display.display();

  // 8) Send data to Blynk (all virtualWrite calls here)
  Blynk.virtualWrite(VPIN_MOISTURE, moisturePct);
  Blynk.virtualWrite(VPIN_TEMP,       temperature);
  Blynk.virtualWrite(VPIN_HUM,        humidity);
  Blynk.virtualWrite(VPIN_PUMP,       pumpOn);
  Blynk.virtualWrite(VPIN_EMER,       emergency);
}

//------------------------------------------------------------------------------
// Blynk handler: remote pump control (V4)
BLYNK_WRITE(VPIN_PUMP) {
  int val = param.asInt();
  if (!emergency) {
    pumpOn = (val == 1);
    digitalWrite(PUMP_PIN, pumpOn ? HIGH : LOW);
  }
  // Echo back to Blynk dashboard
  Blynk.virtualWrite(VPIN_PUMP, pumpOn);
}

//------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Configure pins
  pinMode(RED_LED,      OUTPUT);
  pinMode(WHITE_LED,    OUTPUT);
  pinMode(PUMP_PIN,     OUTPUT);
  pinMode(BUZZER_PIN,   OUTPUT);
  pinMode(BUTTON_STOP,  INPUT_PULLUP);
  pinMode(BUTTON_RESET, INPUT_PULLUP);
  pinMode(WIFI_LED,     OUTPUT);

  dht.begin();

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (1);
  }

  // Show splash on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting to Blynk...");
  display.display();

  // Start Blynk (connects Wi-Fi + Blynk server)
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Blink on-board LED 3 times to indicate Wi-Fi/Blynk connected
  for (int i = 0; i < 3; i++) {
    digitalWrite(WIFI_LED, HIGH);
    delay(300);
    digitalWrite(WIFI_LED, LOW);
    delay(300);
  }

  // Register timer callback: updateReading() every 1 second
  timer.setInterval(1000L, updateReading);
}

//-------------------------------------------------------------------------------
void loop() {
  Blynk.run();
  timer.run();
}