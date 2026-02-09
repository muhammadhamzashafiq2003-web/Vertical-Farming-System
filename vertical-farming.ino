#include "DHT.h"

// -------------------- PIN DEFINITIONS --------------------
#define SOIL_PIN 34     // ADC pin for soil moisture
#define FLOW_PIN 21     // Pulse pin for water flow
#define TDS_PIN 35      // ADC pin for TDS sensor
#define DHT_PIN 22      // Digital pin for DHT11

#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// -------------------- FLOW SENSOR VARIABLES --------------------
volatile int flowPulses = 0;
float flowRate = 0.0;
float totalLiters = 0.0;
unsigned long lastPrintTime = 0;

// -------------------- FLOW ISR --------------------
void IRAM_ATTR flowISR() {
  flowPulses++;
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize pins
  pinMode(SOIL_PIN, INPUT);
  pinMode(TDS_PIN, INPUT);
  pinMode(FLOW_PIN, INPUT_PULLUP);

  // Attach interrupt for water flow
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, RISING);

  // Initialize DHT11
  dht.begin();

  Serial.println("🌱 Vertical Farming – Soil + Flow + TDS + Temp/Humidity (5s Update)");
}

// -------------------- LOOP --------------------
void loop() {

  if (millis() - lastPrintTime >= 5000) {  // 5-second interval

    // ----- FLOW SENSOR -----
    noInterrupts();
    int pulses = flowPulses;
    flowPulses = 0;
    interrupts();

    flowRate = pulses / 7.5 / 5.0;              // L/min over 5 seconds
    float litersUsed = flowRate * (5.0 / 60.0); // L over 5 seconds
    totalLiters += litersUsed;

    // ----- SOIL MOISTURE -----
    int soilValue = analogRead(SOIL_PIN);

    // ----- TDS SENSOR -----
    int tdsRaw = analogRead(TDS_PIN);
    float voltage = tdsRaw * (3.3 / 4095.0);
    float tdsValue = (133.42 * voltage * voltage * voltage
                      - 255.86 * voltage * voltage
                      + 857.39 * voltage) * 0.5; // ppm

    // ----- DHT11 -----
    float tempC = dht.readTemperature();
    float hum = dht.readHumidity();

    // ----- SERIAL OUTPUT -----
    Serial.println("========== 5 SECOND DATA ==========");

    // Soil moisture
    Serial.print("🌱 Soil Moisture: ");
    Serial.println(soilValue);
    if (soilValue > 3000) Serial.println("Status: DRY");
    else if (soilValue > 1800) Serial.println("Status: MOIST");
    else Serial.println("Status: WET");

    // Water flow
    Serial.print("💧 Flow Rate: ");
    Serial.print(flowRate, 2);
    Serial.println(" L/min");

    Serial.print("🚰 Total Water Used: ");
    Serial.print(totalLiters, 3);
    Serial.println(" L");

    // TDS
    Serial.print("⚡ TDS Value: ");
    Serial.print(tdsValue, 0);
    Serial.println(" ppm");

    // DHT11
    if (isnan(tempC) || isnan(hum)) {
      Serial.println("⚠️ Failed to read DHT11!");
    } else {
      Serial.print("🌡️ Temp: ");
      Serial.print(tempC);
      Serial.println(" °C");

      Serial.print("💧 Humidity: ");
      Serial.print(hum);
      Serial.println(" %");
    }

    Serial.println("===================================");
    Serial.println();

    lastPrintTime = millis();
  }
}
