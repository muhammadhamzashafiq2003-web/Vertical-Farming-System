#include "DHT.h"

// -------------------- PIN DEFINITIONS --------------------
#define SOIL_PIN 34     
#define FLOW_PIN 21     
#define TDS_PIN 35      
#define DHT_PIN 22      

#define PH_PIN 32        // NEW
#define TURBIDITY_PIN 33 // NEW

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

  pinMode(SOIL_PIN, INPUT);
  pinMode(TDS_PIN, INPUT);
  pinMode(FLOW_PIN, INPUT_PULLUP);
  pinMode(PH_PIN, INPUT);
  pinMode(TURBIDITY_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, RISING);

  dht.begin();

  Serial.println("🌱 Vertical Farming – FULL SENSOR SYSTEM (5s Update)");
}

// -------------------- LOOP --------------------
void loop() {

  if (millis() - lastPrintTime >= 5000) {

    // ----- FLOW SENSOR -----
    noInterrupts();
    int pulses = flowPulses;
    flowPulses = 0;
    interrupts();

    flowRate = pulses / 7.5 / 5.0;
    float litersUsed = flowRate * (5.0 / 60.0);
    totalLiters += litersUsed;

    // ----- SOIL -----
    int soilValue = analogRead(SOIL_PIN);

    // ----- TDS -----
    int tdsRaw = analogRead(TDS_PIN);
    float tdsVoltage = tdsRaw * (3.3 / 4095.0);
    float tdsValue = (133.42 * tdsVoltage * tdsVoltage * tdsVoltage
                      - 255.86 * tdsVoltage * tdsVoltage
                      + 857.39 * tdsVoltage) * 0.5;

    // ----- pH SENSOR -----
    int phRaw = analogRead(PH_PIN);
    float phVoltage = phRaw * (3.3 / 4095.0);

    // Approximate formula (needs calibration)
    float pHValue = 7 + ((2.5 - phVoltage) / 0.18);

    // ----- TURBIDITY SENSOR -----
    int turbRaw = analogRead(TURBIDITY_PIN);
    float turbVoltage = turbRaw * (3.3 / 4095.0);

    // Approximate NTU conversion
    float turbidity = -1120.4 * turbVoltage * turbVoltage 
                      + 5742.3 * turbVoltage 
                      - 4352.9;

    // ----- DHT11 -----
    float tempC = dht.readTemperature();
    float hum = dht.readHumidity();

    // -------------------- OUTPUT --------------------
    Serial.println("========== 5 SECOND DATA ==========");

    // Soil
    Serial.print("🌱 Soil Moisture: ");
    Serial.println(soilValue);
    if (soilValue > 3000) Serial.println("Status: DRY");
    else if (soilValue > 1800) Serial.println("Status: MOIST");
    else Serial.println("Status: WET");

    // Flow
    Serial.print("💧 Flow Rate: ");
    Serial.print(flowRate, 2);
    Serial.println(" L/min");

    Serial.print("🚰 Total Water Used: ");
    Serial.print(totalLiters, 3);
    Serial.println(" L");

    // TDS
    Serial.print("⚡ TDS: ");
    Serial.print(tdsValue, 0);
    Serial.println(" ppm");

    // pH
    Serial.print("🧪 pH Value: ");
    Serial.println(pHValue, 2);

    // Turbidity
    Serial.print("🌫️ Turbidity: ");
    Serial.print(turbidity, 0);
    Serial.println(" NTU");

    // DHT
    if (isnan(tempC) || isnan(hum)) {
      Serial.println("⚠️ DHT11 Error!");
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
