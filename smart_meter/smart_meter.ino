#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

// =========================
// PIN CONFIGURATION
// =========================
#define ACS712_PIN 34
#define RELAY_PIN 32

// =========================
// WIFI
// =========================
const char* WIFI_SSID = "Oppo";
const char* WIFI_PASSWORD = "7356125156";

// =========================
// API URLS
// =========================
const char* PURCHASE_URL =
  "https://startathon-voltshare-smartmeter.onrender.com/consumer-purchase";

const char* PRODUCER_METRICS_URL =
  "https://startathon-voltshare-smartmeter.onrender.com/meter-metrics/producer";

const char* CONSUMER_METRICS_URL =
  "https://startathon-voltshare-smartmeter.onrender.com/meter-metrics/consumer";

// =========================
// ACS712 CONFIGURATION
// =========================
// ACS712-5A = 0.185 V/A
const float SENSITIVITY = 0.185;

const float ADC_REFERENCE = 3.3;
const float ADC_MAX = 4095.0;

// Ignore tiny sensor noise
const float CURRENT_NOISE_THRESHOLD = 0.10;

// =========================
// ELECTRICAL VALUES
// =========================
const float SUPPLY_VOLTAGE = 9.0;
const float POWER_FACTOR = 1.0;

// =========================
// ADC SAMPLING
// =========================
const int ADC_SAMPLES = 200;
const int ADC_SAMPLE_DELAY_US = 100;

// =========================
// TIMING
// =========================
const unsigned long PURCHASE_INTERVAL = 1000;  // 1 second
const unsigned long METRICS_INTERVAL = 500;    // 500 ms
const unsigned long RELAY_DURATION = 2000;     // 2 seconds

const int LOOP_DELAY_MS = 50;

// =========================
// GLOBAL VARIABLES
// =========================
float zeroVoltage = 0.0;

float measuredCurrent = 0.0;
float measuredVoltage = SUPPLY_VOLTAGE;
float measuredPower = 0.0;
float apparentPower = 0.0;

// Energy stored in Wh
double totalEnergyWh = 0.0;

unsigned long lastEnergyTime = 0;
unsigned long lastPurchaseCheck = 0;
unsigned long lastMetricsSend = 0;

bool currentIsOn = false;

bool relayActive = false;
unsigned long relayStartTime = 0;


// =====================================================
// CONNECT TO WIFI
// =====================================================
void connectWiFi() {

  Serial.println();
  Serial.println("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttempt < 15000) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi connected!");

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println("WiFi connection failed.");
  }
}


// =====================================================
// ENSURE WIFI IS CONNECTED
// =====================================================
void ensureWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println("WiFi disconnected. Reconnecting...");

  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttempt < 10000) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi reconnected.");

  } else {

    Serial.println("WiFi reconnect failed.");
  }
}


// =====================================================
// CALIBRATE ACS712 ZERO CURRENT
// =====================================================
void calibrateCurrentSensor() {

  Serial.println();
  Serial.println("Calibrating ACS712...");
  Serial.println("Make sure NO current is flowing.");

  delay(2000);

  long totalADC = 0;

  const int calibrationSamples = 1000;

  for (int i = 0; i < calibrationSamples; i++) {

    totalADC += analogRead(ACS712_PIN);

    delayMicroseconds(100);
  }

  float averageADC =
    (float)totalADC / calibrationSamples;

  zeroVoltage =
    (averageADC / ADC_MAX) * ADC_REFERENCE;

  Serial.print("Zero ADC: ");
  Serial.println(averageADC, 2);

  Serial.print("Zero voltage: ");
  Serial.print(zeroVoltage, 6);
  Serial.println(" V");

  Serial.println("Calibration complete.");
}


// =====================================================
// READ CURRENT AND CALCULATE POWER + ENERGY
// =====================================================
void readCurrent() {

  long adcSum = 0;

  // Take multiple ADC samples
  for (int i = 0; i < ADC_SAMPLES; i++) {

    adcSum += analogRead(ACS712_PIN);

    delayMicroseconds(ADC_SAMPLE_DELAY_US);
  }

  float averageADC =
    (float)adcSum / ADC_SAMPLES;

  float sensorVoltage =
    (averageADC / ADC_MAX) * ADC_REFERENCE;

  // Calculate current
  float current =
    (sensorVoltage - zeroVoltage) / SENSITIVITY;

  // Remove tiny sensor noise
  if (fabs(current) <= CURRENT_NOISE_THRESHOLD) {

    current = 0.0;
  }

  // Keep current positive
  current = fabs(current);

  measuredCurrent = current;

  // Fixed supply voltage
  measuredVoltage = SUPPLY_VOLTAGE;


  // =========================
  // POWER CALCULATION
  // =========================
  if (measuredCurrent > CURRENT_NOISE_THRESHOLD) {

    measuredPower =
      measuredVoltage *
      measuredCurrent *
      POWER_FACTOR;

    apparentPower =
      measuredVoltage *
      measuredCurrent;

  } else {

    // No current = no power
    measuredPower = 0.0;
    apparentPower = 0.0;
  }


  // =========================
  // ENERGY CALCULATION
  // =========================
  unsigned long now = millis();

  if (lastEnergyTime == 0) {

    lastEnergyTime = now;

  } else {

    unsigned long elapsedMilliseconds =
      now - lastEnergyTime;

    double elapsedHours =
      elapsedMilliseconds / 3600000.0;

    // Only accumulate real energy
    if (measuredCurrent > CURRENT_NOISE_THRESHOLD) {

      // Wh = W × hours
      totalEnergyWh +=
        measuredPower * elapsedHours;
    }

    lastEnergyTime = now;
  }


  currentIsOn =
    measuredCurrent > CURRENT_NOISE_THRESHOLD;


  // =========================
  // SERIAL OUTPUT
  // =========================
  Serial.println();
  Serial.println("========== METER ==========");

  Serial.print("ADC: ");
  Serial.println(averageADC, 2);

  Serial.print("Sensor Voltage: ");
  Serial.print(sensorVoltage, 6);
  Serial.println(" V");

  Serial.print("Supply Voltage: ");
  Serial.print(measuredVoltage, 2);
  Serial.println(" V");

  Serial.print("Current: ");
  Serial.print(measuredCurrent, 4);
  Serial.println(" A");

  Serial.print("Power Factor: ");
  Serial.println(POWER_FACTOR, 2);

  Serial.print("Real Power: ");
  Serial.print(measuredPower, 4);
  Serial.println(" W");

  Serial.print("Apparent Power: ");
  Serial.print(apparentPower, 4);
  Serial.println(" VA");

  Serial.print("Energy: ");
  Serial.print(totalEnergyWh, 12);
  Serial.println(" Wh");

  Serial.print("Current Status: ");

  if (currentIsOn) {

    Serial.println("ON");

  } else {

    Serial.println("OFF");
  }

  Serial.println("============================");
}


// =====================================================
// CHECK CONSUMER PURCHASE
// =====================================================
void checkConsumerPurchase() {

  ensureWiFi();

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println(
      "Skipping purchase check: WiFi unavailable."
    );

    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;

  Serial.println();
  Serial.println("Checking consumer purchase...");

  if (!https.begin(client, PURCHASE_URL)) {

    Serial.println(
      "Unable to begin purchase request."
    );

    return;
  }

  https.setTimeout(3000);

  int httpCode = https.GET();

  Serial.print("Purchase response code: ");
  Serial.println(httpCode);


  if (httpCode == HTTP_CODE_OK) {

    String payload = https.getString();

    Serial.print("Purchase response: ");
    Serial.println(payload);

    DynamicJsonDocument doc(1024);

    DeserializationError error =
      deserializeJson(doc, payload);

    if (!error) {

      if (doc.containsKey("kwh")) {

        float purchasedKWh =
          doc["kwh"].as<float>();

        Serial.print("Purchased energy: ");
        Serial.print(purchasedKWh, 6);
        Serial.println(" kWh");


        // =========================
        // ACTIVATE RELAY
        // =========================
        digitalWrite(RELAY_PIN, HIGH);

        relayActive = true;

        relayStartTime = millis();

        Serial.println("Relay ON.");
      }

    } else {

      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
    }

  } else if (httpCode == HTTP_CODE_NOT_FOUND) {

    Serial.println("No consumer purchase found.");

  } else {

    Serial.print("Purchase request failed: ");
    Serial.println(
      https.errorToString(httpCode)
    );
  }

  https.end();
}


// =====================================================
// UPDATE RELAY
// =====================================================
void updateRelay() {

  if (!relayActive) {
    return;
  }

  unsigned long now = millis();

  if (now - relayStartTime >= RELAY_DURATION) {

    digitalWrite(RELAY_PIN, LOW);

    relayActive = false;

    Serial.println();
    Serial.println("Relay OFF.");
  }
}


// =====================================================
// SEND METRICS TO ONE ENDPOINT
// =====================================================
void sendMetricsToEndpoint(
  const char* endpoint,
  const char* endpointName
) {

  ensureWiFi();

  if (WiFi.status() != WL_CONNECTED) {

    Serial.print("Skipping ");
    Serial.print(endpointName);
    Serial.println(" metrics: WiFi unavailable.");

    return;
  }


  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;

  Serial.println();
  Serial.print("Sending metrics to ");
  Serial.println(endpointName);


  if (!https.begin(client, endpoint)) {

    Serial.print(
      "Unable to begin "
    );

    Serial.print(endpointName);
    Serial.println(" metrics request.");

    return;
  }

  https.setTimeout(3000);

  https.addHeader(
    "Content-Type",
    "application/json"
  );


  // =========================
  // CREATE JSON
  // =========================
  DynamicJsonDocument doc(512);

  // Real measured power
  doc["power"] = measuredPower;

  // Real accumulated energy in Wh
  doc["energy"] = totalEnergyWh;

  // Supply voltage
  doc["voltage"] = measuredVoltage;

  // Power factor
  doc["powerFactor"] = POWER_FACTOR;


  String jsonPayload;

  serializeJson(
    doc,
    jsonPayload
  );


  Serial.print("POST body: ");
  Serial.println(jsonPayload);


  // =========================
  // POST
  // =========================
  int httpCode =
    https.POST(jsonPayload);


  Serial.print(
    endpointName
  );

  Serial.print(
    " response code: "
  );

  Serial.println(httpCode);


  // =========================
  // RESPONSE HANDLING
  // =========================
  if (httpCode > 0) {

    if (httpCode == 200 ||
        httpCode == 201 ||
        httpCode == 202 ||
        httpCode == 204) {

      Serial.print(
        endpointName
      );

      Serial.println(
        " metrics sent successfully."
      );

    } else {

      String response =
        https.getString();

      Serial.print(
        endpointName
      );

      Serial.print(
        " server response: "
      );

      Serial.println(response);
    }

  } else {

    Serial.print(
      endpointName
    );

    Serial.print(
      " POST failed: "
    );

    Serial.println(
      https.errorToString(httpCode)
    );
  }


  https.end();
}


// =====================================================
// SEND METRICS TO BOTH PRODUCER AND CONSUMER
// =====================================================
void sendMeterMetrics() {

  // =========================
  // PRODUCER
  // =========================
  sendMetricsToEndpoint(
    PRODUCER_METRICS_URL,
    "PRODUCER"
  );


  // =========================
  // CONSUMER
  // =========================
  sendMetricsToEndpoint(
    CONSUMER_METRICS_URL,
    "CONSUMER"
  );
}


// =====================================================
// SETUP
// =====================================================
void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("       ESP32 SMART METER");
  Serial.println("================================");


  // =========================
  // RELAY
  // =========================
  pinMode(
    RELAY_PIN,
    OUTPUT
  );

  // Start relay OFF
  digitalWrite(
    RELAY_PIN,
    LOW
  );


  // =========================
  // ADC
  // =========================
  analogReadResolution(12);

  analogSetPinAttenuation(
    ACS712_PIN,
    ADC_11db
  );


  // =========================
  // WIFI
  // =========================
  connectWiFi();


  // =========================
  // ACS712 CALIBRATION
  // =========================
  calibrateCurrentSensor();


  // =========================
  // INITIALIZE TIMERS
  // =========================
  lastEnergyTime =
    millis();

  // Make first purchase request happen immediately
  lastPurchaseCheck =
    millis() - PURCHASE_INTERVAL;

  // Make first metrics POST happen immediately
  lastMetricsSend =
    millis() - METRICS_INTERVAL;


  Serial.println();
  Serial.println("Smart meter ready.");
  Serial.println();
}


// =====================================================
// MAIN LOOP
// =====================================================
void loop() {

  unsigned long now = millis();


  // =========================
  // READ SENSOR
  // =========================
  readCurrent();


  // =========================
  // UPDATE RELAY
  // =========================
  updateRelay();


  // =========================
  // CHECK PURCHASE
  // =========================
  if (now - lastPurchaseCheck >=
      PURCHASE_INTERVAL) {

    lastPurchaseCheck = now;

    checkConsumerPurchase();
  }


  // =========================
  // SEND METRICS
  // =========================
  if (now - lastMetricsSend >=
      METRICS_INTERVAL) {

    lastMetricsSend = now;

    // Sends to BOTH:
    // /meter-metrics/producer
    // /meter-metrics/consumer
    sendMeterMetrics();
  }


  delay(LOOP_DELAY_MS);
}