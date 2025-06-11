#include <WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// WiFi + MQTT
char ssid[] = "";
char pass[] = "";

#define THINGSBOARD_SERVER "demo.thingsboard.io"
#define TOKEN              "RoSjfcHzFBSP2AUVQQOn"

WiFiClient espClient;
PubSubClient client(espClient);
Ticker readingTimer;

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// DHT
#define DHTPIN   14
#define DHTTYPE  DHT11
DHT dht(DHTPIN, DHTTYPE);

// Pins
#define SENSOR_PIN    34
#define RED_LED       18
#define WHITE_LED     19
#define PUMP_PIN      25
#define BUZZER_PIN    26
#define BUTTON_STOP   32
#define BUTTON_RESET  33
#define WIFI_LED      2

// Moisture calibration
#define DRY_RAW       3500
#define WET_RAW       1200

// State
volatile bool pumpOn       = false;
volatile bool manualPump   = false;
volatile bool emergency    = false;
int moisturePct            = 0;
float temperature          = 0.0;
float humidity             = 0.0;

//----------------------------------------------------------------
// WiFi
void connectWiFi() {
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
}

// MQTT
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to ThingsBoard...");
    if (client.connect("ESP32Client", TOKEN, NULL)) {
      Serial.println("connected");
      client.subscribe("v1/devices/me/rpc/request/+");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

//----------------------------------------------------------------
// RPC Callback
void callback(char* topic, byte* payload, unsigned int length) {
  String data;
  for (int i = 0; i < length; i++) {
    data += (char)payload[i];
  }

  String topicStr = String(topic);
  if (data.indexOf("method") > 0) {
    if (data.indexOf("setPump") > 0) {
      if (!emergency) {
        manualPump = data.indexOf("true") > 0;
        pumpOn = manualPump;
      }
    } else if (data.indexOf("emergencyStop") > 0) {
      emergency = data.indexOf("true") > 0;
      if (emergency) {
        pumpOn = false;
        manualPump = false;
        noTone(BUZZER_PIN);
      }
    }
  }
}

//----------------------------------------------------------------
// Buttons
void checkButtons() {
  static bool lastStop = HIGH, lastReset = HIGH;
  bool nowStop = digitalRead(BUTTON_STOP);
  bool nowReset = digitalRead(BUTTON_RESET);

  if (lastStop == HIGH && nowStop == LOW) {
    emergency = true;
    pumpOn = false;
    manualPump = false;
    noTone(BUZZER_PIN);
    Serial.println(">> EMERGENCY STOP (Button)");
  }

  if (lastReset == HIGH && nowReset == LOW) {
    emergency = false;
    Serial.println(">> EMERGENCY RESET (Button)");
  }

  lastStop = nowStop;
  lastReset = nowReset;
}

//----------------------------------------------------------------
// Buzzer beep
void wetBeep() {
  tone(BUZZER_PIN, 2000, 150);
  delay(200);
  tone(BUZZER_PIN, 2000, 150);
}

//----------------------------------------------------------------
// Sensor + Logic + Display + MQTT
void updateReading() {
  checkButtons();

  // Moisture sensor
  int raw = analogRead(SENSOR_PIN);
  raw = constrain(raw, WET_RAW, DRY_RAW);
  moisturePct = map(raw, DRY_RAW, WET_RAW, 0, 100);

  // DHT
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  // Pump Logic
  if (emergency) {
    pumpOn = false;
    noTone(BUZZER_PIN);
  } else {
    if (!manualPump) {
      if (!pumpOn && moisturePct < 30) pumpOn = true;
      else if (pumpOn && moisturePct >= 70) {
        pumpOn = false;
        wetBeep();
      }
    }

    if (moisturePct < 30) tone(BUZZER_PIN, 1000);
    else if (moisturePct < 70) noTone(BUZZER_PIN);
  }

  digitalWrite(PUMP_PIN, pumpOn ? HIGH : LOW);
  digitalWrite(RED_LED, (moisturePct < 30) ? HIGH : LOW);
  digitalWrite(WHITE_LED, (moisturePct >= 70) ? HIGH : LOW);

  // OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.printf("Moisture: %3d%%\n", moisturePct);
  display.printf("Temp:     %.1f C\n", temperature);
  display.printf("Humidity: %.1f %%\n", humidity);
  display.printf("Pump:     %s\n", pumpOn ? "ON" : "OFF");

  if (moisturePct < 30) display.println("Soil: DRY");
  else if (moisturePct >= 70) display.println("Soil: WET");
  else display.println("Soil: NORMAL");

  if (emergency) display.println("!! EMERGENCY !!");
  display.display();

  // MQTT Publish
  String payload = "{";
  payload += "\"moisture\":" + String(moisturePct) + ",";
  payload += "\"temperature\":" + String(temperature) + ",";
  payload += "\"humidity\":" + String(humidity) + ",";
  payload += "\"pumpOn\":" + String(pumpOn ? "true" : "false") + ",";
  payload += "\"emergency\":" + String(emergency ? "true" : "false");
  payload += "}";

  client.publish("v1/devices/me/telemetry", payload.c_str());
}

//----------------------------------------------------------------
// Setup
void setup() {
  Serial.begin(115200);

  pinMode(RED_LED, OUTPUT);
  pinMode(WHITE_LED, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_STOP, INPUT_PULLUP);
  pinMode(BUTTON_RESET, INPUT_PULLUP);
  pinMode(WIFI_LED, OUTPUT);

  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
    while (1);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println("Connecting to WiFi...");
  display.display();

  connectWiFi();
  client.setServer(THINGSBOARD_SERVER, 1883);
  client.setCallback(callback);
  connectMQTT();

  for (int i = 0; i < 3; i++) {
    digitalWrite(WIFI_LED, HIGH); delay(300);
    digitalWrite(WIFI_LED, LOW); delay(300);
  }

  readingTimer.attach(1.0, updateReading);  // Every 1 second
}

//----------------------------------------------------------------
void loop() {
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();
}
