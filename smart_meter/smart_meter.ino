#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

// =====================================================
// PIN CONFIGURATION
// =====================================================

#define ACS712_PIN 34
#define RELAY_PIN 32


// =====================================================
// WIFI / SERVER CONFIGURATION
// =====================================================

const char* WIFI_SSID = "Oppo";
const char* WIFI_PASSWORD = "7356125156";

const char* PURCHASE_URL =
  "https://startathon-voltshare-smartmeter.onrender.com/consumer-purchase";


// =====================================================
// ACS712 CONFIGURATION
// =====================================================

const float SENSITIVITY = 0.185;
const float ADC_REFERENCE = 3.3;
const float ADC_MAX = 4095.0;
const float CURRENT_NOISE_THRESHOLD = 0.10;


// =====================================================
// TIMING CONFIGURATION
// =====================================================

const int ADC_SAMPLES = 200;
const int ADC_SAMPLE_DELAY_US = 100;

const unsigned long PURCHASE_INTERVAL = 1000;
const unsigned long RELAY_DURATION = 2000;


// =====================================================
// TASK CONFIGURATION
// =====================================================

const int SENSOR_TASK_DELAY_MS = 50;
const int NETWORK_TASK_DELAY_MS = 20;
const int RELAY_TASK_DELAY_MS = 10;


// =====================================================
// GLOBAL VARIABLES
// =====================================================

float zeroVoltage = 0.0;


// =====================================================
// SHARED STATE
// =====================================================

bool currentIsOn = false;

bool relayTrigger = false;
bool relayActive = false;

unsigned long relayStartTime = 0;


// =====================================================
// MUTEX
// =====================================================

SemaphoreHandle_t stateMutex;


// =====================================================
// TIMER
// =====================================================

unsigned long lastPurchaseCheck = 0;


// =====================================================
// WIFI CONNECTION
// =====================================================

void connectWiFi() {

  Serial.println();
  Serial.println("==============================");
  Serial.println("Starting WiFi connection...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 40) {

    vTaskDelay(pdMS_TO_TICKS(250));

    Serial.print(".");

    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi connected!");

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

  } else {

    Serial.println("WiFi connection FAILED!");
  }

  Serial.println("==============================");
}


// =====================================================
// ENSURE WIFI CONNECTION
// =====================================================

bool ensureWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println();
  Serial.println("WiFi disconnected.");
  Serial.println("Reconnecting...");

  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 5000) {

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi reconnected!");

    return true;
  }

  Serial.println("WiFi reconnect failed.");

  return false;
}


// =====================================================
// CHECK CONSUMER PURCHASE
// =====================================================

void checkConsumerPurchase() {

  if (!ensureWiFi()) {
    return;
  }

  Serial.println();
  Serial.println("Checking consumer purchase...");

  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient http;

  if (!http.begin(client, PURCHASE_URL)) {

    Serial.println("Failed to start purchase request.");

    return;
  }

  http.setConnectTimeout(3000);
  http.setTimeout(3000);

  int httpCode = http.GET();

  Serial.print("Purchase response code: ");
  Serial.println(httpCode);


  // ===================================================
  // NO PURCHASE
  // ===================================================

  if (httpCode == 404) {

    Serial.println("No consumer purchase found.");

    http.end();

    return;
  }


  // ===================================================
  // SERVER ERROR
  // ===================================================

  if (httpCode <= 0) {

    Serial.print("Purchase request failed: ");
    Serial.println(http.errorToString(httpCode));

    http.end();

    return;
  }


  // ===================================================
  // GET JSON
  // ===================================================

  String jsonResponse = http.getString();

  http.end();

  Serial.print("Purchase JSON: ");
  Serial.println(jsonResponse);


  // ===================================================
  // PARSE JSON
  // ===================================================

  JsonDocument doc;

  DeserializationError error =
    deserializeJson(doc, jsonResponse);

  if (error) {

    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());

    return;
  }


  // ===================================================
  // GET KWH
  // ===================================================

  if (!doc["kwh"].is<float>() &&
      !doc["kwh"].is<int>() &&
      !doc["kwh"].is<double>()) {

    Serial.println("JSON does not contain valid kwh.");

    return;
  }

  float kwh = doc["kwh"].as<float>();

  Serial.print("KWH received: ");
  Serial.println(kwh, 4);


  // ===================================================
  // REQUEST RELAY
  // ===================================================

  Serial.println("================================");
  Serial.println("PURCHASE DETECTED!");
  Serial.println("Requesting Relay ON");
  Serial.println("================================");


  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

    relayTrigger = true;

    xSemaphoreGive(stateMutex);
  }
}


// =====================================================
// READ CURRENT
// =====================================================

float readCurrent() {

  long total = 0;

  for (int i = 0; i < ADC_SAMPLES; i++) {

    total += analogRead(ACS712_PIN);

    delayMicroseconds(ADC_SAMPLE_DELAY_US);
  }


  float averageADC =
    total / (float)ADC_SAMPLES;


  float voltage =
    (averageADC / ADC_MAX) * ADC_REFERENCE;


  float current =
    (voltage - zeroVoltage) / SENSITIVITY;


  // Remove noise
  if (current > -CURRENT_NOISE_THRESHOLD &&
      current < CURRENT_NOISE_THRESHOLD) {

    current = 0;
  }


  // Original sign reversal
  current = -current;


  Serial.println();
  Serial.println("------------------------------");

  Serial.print("ADC = ");
  Serial.println(averageADC);

  Serial.print("Voltage = ");
  Serial.print(voltage, 4);
  Serial.println(" V");

  Serial.print("Current = ");
  Serial.print(current, 3);
  Serial.println(" A");

  Serial.println("------------------------------");


  return current;
}


// =====================================================
// SENSOR TASK
// =====================================================

void sensorTask(void* parameter) {

  Serial.println("Sensor task started.");

  while (true) {

    float current = readCurrent();

    bool newCurrentState =
      fabs(current) >= CURRENT_NOISE_THRESHOLD;


    // Save current state
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {

      currentIsOn = newCurrentState;

      xSemaphoreGive(stateMutex);
    }


    if (newCurrentState) {

      Serial.println("CURRENT: ON");

    } else {

      Serial.println("CURRENT: OFF");
    }


    vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_DELAY_MS));
  }
}


// =====================================================
// RELAY TASK
// =====================================================

void relayTask(void* parameter) {

  Serial.println("Relay task started.");

  while (true) {

    bool shouldTrigger = false;


    // =================================================
    // CHECK FOR RELAY REQUEST
    // =================================================

    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {

      if (relayTrigger) {

        relayTrigger = false;

        shouldTrigger = true;
      }

      xSemaphoreGive(stateMutex);
    }


    // =================================================
    // TURN RELAY ON
    // =================================================

    if (shouldTrigger) {

      digitalWrite(RELAY_PIN, HIGH);

      Serial.println("Relay ON");


      if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {

        relayActive = true;

        relayStartTime = millis();

        xSemaphoreGive(stateMutex);
      }
    }


    // =================================================
    // CHECK RELAY TIMER
    // =================================================

    bool shouldTurnOff = false;


    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {

      if (relayActive &&
          millis() - relayStartTime >= RELAY_DURATION) {

        relayActive = false;

        shouldTurnOff = true;
      }

      xSemaphoreGive(stateMutex);
    }


    // =================================================
    // TURN RELAY OFF
    // =================================================

    if (shouldTurnOff) {

      digitalWrite(RELAY_PIN, LOW);

      Serial.println("Relay OFF");
    }


    vTaskDelay(pdMS_TO_TICKS(RELAY_TASK_DELAY_MS));
  }
}


// =====================================================
// NETWORK TASK
// =====================================================
//
// ONLY checks consumer purchases.
// NO ON/OFF status is sent to the server.
//

void networkTask(void* parameter) {

  Serial.println("Network task started.");

  // Allow immediate first purchase check
  lastPurchaseCheck =
    millis() - PURCHASE_INTERVAL;


  while (true) {

    unsigned long now = millis();


    // =================================================
    // CHECK PURCHASE
    // =================================================

    if (now - lastPurchaseCheck >= PURCHASE_INTERVAL) {

      lastPurchaseCheck = now;

      checkConsumerPurchase();
    }


    // =================================================
    // GIVE CPU TO OTHER TASKS
    // =================================================

    vTaskDelay(pdMS_TO_TICKS(NETWORK_TASK_DELAY_MS));
  }
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);

  delay(500);


  // ===================================================
  // WIFI
  // ===================================================

  connectWiFi();


  // ===================================================
  // ADC CONFIGURATION
  // ===================================================

  analogReadResolution(12);

  analogSetPinAttenuation(
    ACS712_PIN,
    ADC_11db
  );


  // ===================================================
  // CALIBRATION
  // ===================================================

  Serial.println();
  Serial.println("Calibrating zero current...");
  Serial.println("Make sure NO current is flowing.");

  delay(1000);

  long total = 0;

  const int CALIBRATION_SAMPLES = 1000;

  for (int i = 0;
       i < CALIBRATION_SAMPLES;
       i++) {

    total += analogRead(ACS712_PIN);

    delayMicroseconds(100);
  }


  float averageADC =
    total / (float)CALIBRATION_SAMPLES;


  zeroVoltage =
    (averageADC / ADC_MAX) * ADC_REFERENCE;


  Serial.print("Zero ADC = ");
  Serial.println(averageADC);

  Serial.print("Zero Voltage = ");
  Serial.print(zeroVoltage, 4);
  Serial.println(" V");

  Serial.println("Calibration complete.");


  // ===================================================
  // CREATE MUTEX
  // ===================================================

  stateMutex = xSemaphoreCreateMutex();

  if (stateMutex == NULL) {

    Serial.println("ERROR: Failed to create mutex!");

    while (true) {
      delay(1000);
    }
  }


  // ===================================================
  // SENSOR TASK
  // ===================================================

  xTaskCreatePinnedToCore(
    sensorTask,
    "SensorTask",
    4096,
    NULL,
    2,
    NULL,
    0
  );


  // ===================================================
  // RELAY TASK
  // ===================================================

  xTaskCreatePinnedToCore(
    relayTask,
    "RelayTask",
    4096,
    NULL,
    3,
    NULL,
    0
  );


  // ===================================================
  // NETWORK TASK
  // ===================================================

  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    8192,
    NULL,
    1,
    NULL,
    1
  );


  // ===================================================
  // READY
  // ===================================================

  Serial.println();
  Serial.println("ESP32 is ready.");
  Serial.println("================================");
  Serial.println("Multithreading enabled:");
  Serial.println("Sensor  -> Core 0");
  Serial.println("Relay   -> Core 0");
  Serial.println("Network -> Core 1");
  Serial.println("================================");
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  // All work is handled by FreeRTOS tasks.

  vTaskDelay(pdMS_TO_TICKS(1000));
}