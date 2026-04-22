#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

// =====================================================
// WIFI + FIREBASE SETTINGS
// =====================================================
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Realtime Database base URL from your screenshot
const char* FIREBASE_HOST = "https://vertical-farming-5ac03-default-rtdb.firebaseio.com";

// If your Firebase rules require auth, put token here.
// If database is open for testing, keep it empty: ""
const char* FIREBASE_AUTH = "";

// Exact farm node from your screenshot
const char* FARM_NODE = "farmsByEmail/armaghanali26@gmail_com";

// =====================================================
// PIN DEFINITIONS
// =====================================================
#define SOIL_PIN       34
#define FLOW_PIN       21
#define TDS_PIN        35
#define DHT_PIN        22
#define PH_PIN         32
#define TURBIDITY_PIN  33

// Mist pump relay output pin (change if your hardware uses another pin)
#define MIST_PUMP_PIN  25

#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// =====================================================
// FLOW SENSOR VARIABLES
// =====================================================
volatile int flowPulses = 0;
float flowRate = 0.0;
float totalLiters = 0.0;
unsigned long lastSensorUpdate = 0;
const unsigned long SENSOR_INTERVAL = 5000;

// =====================================================
// FLOW ISR
// =====================================================
void IRAM_ATTR flowISR() {
  flowPulses++;
}

// =====================================================
// WIFI
// =====================================================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - startAttempt > 20000) {
      Serial.println("\nWiFi connection timeout. Restarting...");
      ESP.restart();
    }
  }

  Serial.println();
  Serial.print("WiFi connected. IP: ");
  Serial.println(WiFi.localIP());
}

// =====================================================
// FIREBASE URL BUILDER
// =====================================================
String buildFirebaseURL(const String& path) {
  String url = String(FIREBASE_HOST) + "/" + path + ".json";
  if (strlen(FIREBASE_AUTH) > 0) {
    url += "?auth=" + String(FIREBASE_AUTH);
  }
  return url;
}

// =====================================================
// HTTPS GET STRING
// =====================================================
String httpsGET(const String& url, int &httpCode) {
  WiFiClientSecure client;
  client.setInsecure(); // quick testing; for production prefer certificate validation

  HTTPClient https;
  String payload = "";

  if (https.begin(client, url)) {
    httpCode = https.GET();
    if (httpCode > 0) {
      payload = https.getString();
    } else {
      Serial.print("GET failed: ");
      Serial.println(https.errorToString(httpCode));
    }
    https.end();
  } else {
    Serial.println("Unable to begin HTTPS GET");
    httpCode = -1;
  }

  return payload;
}

// =====================================================
// HTTPS PATCH JSON
// =====================================================
bool httpsPATCH(const String& url, const String& json) {
  WiFiClientSecure client;
  client.setInsecure(); // quick testing; for production prefer certificate validation

  HTTPClient https;
  bool ok = false;

  if (https.begin(client, url)) {
    https.addHeader("Content-Type", "application/json");
    int httpCode = https.sendRequest("PATCH", (uint8_t*)json.c_str(), json.length());

    Serial.print("PATCH -> HTTP ");
    Serial.println(httpCode);

    if (httpCode > 0) {
      String response = https.getString();
      Serial.println("PATCH response: " + response);
      ok = (httpCode >= 200 && httpCode < 300);
    } else {
      Serial.print("PATCH failed: ");
      Serial.println(https.errorToString(httpCode));
    }
    https.end();
  } else {
    Serial.println("Unable to begin HTTPS PATCH");
  }

  return ok;
}

// =====================================================
// READ MIST PUMP COMMAND FROM FIREBASE
// =====================================================
void syncMistPumpFromFirebase() {
  String path = String(FARM_NODE) + "/actuators/mistPump/lastCommand/state";
  String url = buildFirebaseURL(path);

  int httpCode = 0;
  String response = httpsGET(url, httpCode);

  if (httpCode == 200) {
    response.trim();
    response.replace("\"", "");

    Serial.print("MistPump command from Firebase: ");
    Serial.println(response);

    if (response == "ON") {
      digitalWrite(MIST_PUMP_PIN, HIGH);
    } else {
      digitalWrite(MIST_PUMP_PIN, LOW);
    }
  } else {
    Serial.println("Could not read mistPump state from Firebase");
  }
}

// =====================================================
// SEND SENSOR DATA TO FIREBASE
// =====================================================
void sendSensorsToFirebase(float humidity,
                           float temperature,
                           int soilMoisture,
                           float tds,
                           float ph,
                           float turbidity,
                           float waterFlow) {
  DynamicJsonDocument doc(512);

  doc["humidity"]     = isnan(humidity) ? 0 : humidity;
  doc["temperature"]  = isnan(temperature) ? 0 : temperature;
  doc["soilMoisture"] = soilMoisture;
  doc["tds"]          = round(tds * 100.0) / 100.0;
  doc["ph"]           = round(ph * 100.0) / 100.0;
  doc["turbidity"]    = round(turbidity * 100.0) / 100.0;
  doc["waterFlow"]    = round(waterFlow * 100.0) / 100.0;

  // Firebase server timestamp placeholder
  JsonObject ts = doc.createNestedObject("updatedAt");
  ts[".sv"] = "timestamp";

  String json;
  serializeJson(doc, json);

  String path = String(FARM_NODE) + "/sensors";
  String url = buildFirebaseURL(path);

  Serial.println("Sending sensor JSON:");
  Serial.println(json);

  bool ok = httpsPATCH(url, json);

  if (ok) {
    Serial.println("Sensor data uploaded to Firebase successfully");
  } else {
    Serial.println("Sensor data upload failed");
  }
}

// =====================================================
// SENSOR READING
// =====================================================
void readAndUploadSensors() {
  // ---------- FLOW ----------
  noInterrupts();
  int pulses = flowPulses;
  flowPulses = 0;
  interrupts();

  flowRate = pulses / 7.5 / 5.0;              // L/min over 5s window
  float litersUsed = flowRate * (5.0 / 60.0); // liters in 5 seconds
  totalLiters += litersUsed;

  // ---------- SOIL ----------
  int soilValue = analogRead(SOIL_PIN);

  // ---------- TDS ----------
  int tdsRaw = analogRead(TDS_PIN);
  float tdsVoltage = tdsRaw * (3.3 / 4095.0);
  float tdsValue = (133.42 * tdsVoltage * tdsVoltage * tdsVoltage
                  - 255.86 * tdsVoltage * tdsVoltage
                  + 857.39 * tdsVoltage) * 0.5;

  // ---------- pH ----------
  int phRaw = analogRead(PH_PIN);
  float phVoltage = phRaw * (3.3 / 4095.0);
  float pHValue = 7 + ((2.5 - phVoltage) / 0.18); // requires calibration

  // ---------- TURBIDITY ----------
  int turbRaw = analogRead(TURBIDITY_PIN);
  float turbVoltage = turbRaw * (3.3 / 4095.0);
  float turbidity = -1120.4 * turbVoltage * turbVoltage
                  + 5742.3 * turbVoltage
                  - 4352.9;

  if (turbidity < 0) turbidity = 0;

  // ---------- DHT ----------
  float tempC = dht.readTemperature();
  float hum   = dht.readHumidity();

  // ---------- SERIAL OUTPUT ----------
  Serial.println("========== 5 SECOND DATA ==========");
  Serial.print("Soil Moisture: "); Serial.println(soilValue);
  Serial.print("Flow Rate: "); Serial.print(flowRate, 2); Serial.println(" L/min");
  Serial.print("Total Water Used: "); Serial.print(totalLiters, 3); Serial.println(" L");
  Serial.print("TDS: "); Serial.print(tdsValue, 0); Serial.println(" ppm");
  Serial.print("pH Value: "); Serial.println(pHValue, 2);
  Serial.print("Turbidity: "); Serial.print(turbidity, 0); Serial.println(" NTU");

  if (isnan(tempC) || isnan(hum)) {
    Serial.println("DHT11 Error!");
  } else {
    Serial.print("Temp: "); Serial.print(tempC); Serial.println(" C");
    Serial.print("Humidity: "); Serial.print(hum); Serial.println(" %");
  }
  Serial.println("===================================");

  // ---------- FIREBASE UPLOAD ----------
  sendSensorsToFirebase(hum, tempC, soilValue, tdsValue, pHValue, turbidity, flowRate);
}

// =====================================================
// OPTIONAL: SEND LOCAL ACTUATOR STATE BACK TO FIREBASE
// =====================================================
void updateMistPumpStateToFirebase(bool isOn) {
  DynamicJsonDocument doc(128);
  doc["state"] = isOn ? "ON" : "OFF";

  String json;
  serializeJson(doc, json);

  String path = String(FARM_NODE) + "/actuators/mistPump";
  String url = buildFirebaseURL(path);

  httpsPATCH(url, json);
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(SOIL_PIN, INPUT);
  pinMode(TDS_PIN, INPUT);
  pinMode(FLOW_PIN, INPUT_PULLUP);
  pinMode(PH_PIN, INPUT);
  pinMode(TURBIDITY_PIN, INPUT);

  pinMode(MIST_PUMP_PIN, OUTPUT);
  digitalWrite(MIST_PUMP_PIN, LOW);

  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, RISING);

  dht.begin();
  connectWiFi();

  Serial.println("Vertical Farming Firebase System Started");
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    connectWiFi();
  }

  // Read mist pump command from Firebase frequently
  syncMistPumpFromFirebase();

  // Upload sensors every 5 seconds
  if (millis() - lastSensorUpdate >= SENSOR_INTERVAL) {
    readAndUploadSensors();

    bool pumpState = digitalRead(MIST_PUMP_PIN);
    updateMistPumpStateToFirebase(pumpState);

    lastSensorUpdate = millis();
  }

  delay(1000);
}
