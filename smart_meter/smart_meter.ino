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

const char* METRICS_URL =
  "https://startathon-voltshare-smartmeter.onrender.com/meter-metrics/consumer";


// =====================================================
// ACS712 CONFIGURATION
// =====================================================

const float SENSITIVITY = 0.185;
const float ADC_REFERENCE = 3.3;
const float ADC_MAX = 4095.0;
const float CURRENT_NOISE_THRESHOLD = 0.10;


// =====================================================
// PRODUCER METRIC CONFIGURATION
// =====================================================

// Actual voltage to report
const float PRODUCER_VOLTAGE = 5.0;

// Power factor to report
const float POWER_FACTOR = 0.95;

// Accumulated producer energy in kWh
float producerEnergyKWh = 0.0;


// =====================================================
// TIMING CONFIGURATION
// =====================================================

const int ADC_SAMPLES = 200;
const int ADC_SAMPLE_DELAY_US = 100;

const unsigned long PURCHASE_INTERVAL = 1000;
const unsigned long RELAY_DURATION = 2000;

const unsigned long METRICS_INTERVAL = 1000;


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

// Latest current measured by sensor task
float latestCurrent = 0.0;


// =====================================================
// MUTEX
// =====================================================

SemaphoreHandle_t stateMutex;


// =====================================================
// TIMERS
// =====================================================

unsigned long lastPurchaseCheck = 0;
unsigned long lastMetricsPost = 0;
unsigned long lastEnergyUpdate = 0;


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

  Serial.print("ACS712 Output Voltage = ");
  Serial.print(voltage, 4);
  Serial.println(" V");

  Serial.print("Current = ");
  Serial.print(current, 3);
  Serial.println(" A");

  Serial.println("------------------------------");


  return current;
}


// =====================================================
// POST PRODUCER METRICS
// =====================================================

void postProducerMetrics(float current) {

  if (!ensureWiFi()) {
    return;
  }


  // ===================================================
  // CALCULATE POWER
  // ===================================================

  float power =
    PRODUCER_VOLTAGE *
    fabs(current) *
    POWER_FACTOR;


  // ===================================================
  // CALCULATE ENERGY
  // ===================================================

  unsigned long now = millis();

  if (lastEnergyUpdate == 0) {

    lastEnergyUpdate = now;
  }

  float elapsedHours =
    (now - lastEnergyUpdate) / 3600000.0;

  // Power is in watts.
  // Convert watts to kW and multiply by hours.

  producerEnergyKWh +=
    (power / 1000.0) * elapsedHours;

  lastEnergyUpdate = now;


  // ===================================================
  // CREATE HTTPS CLIENT
  // ===================================================

  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient http;


  if (!http.begin(client, METRICS_URL)) {

    Serial.println(
      "Failed to start producer metrics request."
    );

    return;
  }


  // ===================================================
  // HTTP HEADERS
  // ===================================================

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  http.setConnectTimeout(3000);
  http.setTimeout(3000);


  // ===================================================
  // CREATE JSON
  // ===================================================

  JsonDocument doc;

  doc["power"] = power;
  doc["energy"] = producerEnergyKWh;
  doc["voltage"] = PRODUCER_VOLTAGE;
  doc["powerFactor"] = POWER_FACTOR;


  String requestBody;

  serializeJson(doc, requestBody);


  // ===================================================
  // DEBUG OUTPUT
  // ===================================================

  Serial.println();
  Serial.println("==============================");
  Serial.println("PRODUCER METRICS");
  Serial.println("==============================");

  Serial.print("Current: ");
  Serial.print(current, 3);
  Serial.println(" A");

  Serial.print("Power: ");
  Serial.print(power, 2);
  Serial.println(" W");

  Serial.print("Energy: ");
  Serial.print(producerEnergyKWh, 6);
  Serial.println(" kWh");

  Serial.print("Voltage: ");
  Serial.print(PRODUCER_VOLTAGE, 2);
  Serial.println(" V");

  Serial.print("Power Factor: ");
  Serial.println(POWER_FACTOR, 2);

  Serial.print("POST body: ");
  Serial.println(requestBody);


  // ===================================================
  // SEND POST
  // ===================================================

  int httpCode =
    http.POST(requestBody);


  Serial.print(
    "Producer metrics response code: "
  );

  Serial.println(httpCode);


  // ===================================================
  // RESPONSE
  // ===================================================

  if (httpCode > 0) {

    String response =
      http.getString();

    Serial.print(
      "Producer metrics response: "
    );

    Serial.println(response);

  } else {

    Serial.print(
      "Producer metrics POST failed: "
    );

    Serial.println(
      http.errorToString(httpCode)
    );
  }


  Serial.println("==============================");

  http.end();
}


// =====================================================
// SENSOR TASK
// =====================================================

void sensorTask(void* parameter) {

  Serial.println("Sensor task started.");

  while (true) {

    float current = readCurrent();


    // =================================================
    // SAVE LATEST CURRENT
    // =================================================

    if (xSemaphoreTake(
          stateMutex,
          pdMS_TO_TICKS(10)
        ) == pdTRUE) {

      latestCurrent = current;

      xSemaphoreGive(stateMutex);
    }


    // =================================================
    // DETERMINE CURRENT STATE
    // =================================================

    bool newCurrentState =
      fabs(current) >= CURRENT_NOISE_THRESHOLD;


    // Save current state
    if (xSemaphoreTake(
          stateMutex,
          pdMS_TO_TICKS(10)
        ) == pdTRUE) {

      currentIsOn = newCurrentState;

      xSemaphoreGive(stateMutex);
    }


    if (newCurrentState) {

      Serial.println("CURRENT: ON");

    } else {

      Serial.println("CURRENT: OFF");
    }


    vTaskDelay(
      pdMS_TO_TICKS(SENSOR_TASK_DELAY_MS)
    );
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

    if (xSemaphoreTake(
          stateMutex,
          pdMS_TO_TICKS(10)
        ) == pdTRUE) {

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


      if (xSemaphoreTake(
            stateMutex,
            pdMS_TO_TICKS(10)
          ) == pdTRUE) {

        relayActive = true;

        relayStartTime = millis();

        xSemaphoreGive(stateMutex);
      }
    }


    // =================================================
    // CHECK RELAY TIMER
    // =================================================

    bool shouldTurnOff = false;


    if (xSemaphoreTake(
          stateMutex,
          pdMS_TO_TICKS(10)
        ) == pdTRUE) {

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


    vTaskDelay(
      pdMS_TO_TICKS(RELAY_TASK_DELAY_MS)
    );
  }
}


// =====================================================
// NETWORK TASK
// =====================================================

void networkTask(void* parameter) {

  Serial.println("Network task started.");

  // Allow immediate first purchase check
  lastPurchaseCheck =
    millis() - PURCHASE_INTERVAL;

  // Allow immediate first metrics POST
  lastMetricsPost =
    millis() - METRICS_INTERVAL;

  lastEnergyUpdate = millis();


  while (true) {

    unsigned long now = millis();


    // =================================================
    // CHECK PURCHASE
    // =================================================

    if (now - lastPurchaseCheck >=
        PURCHASE_INTERVAL) {

      lastPurchaseCheck = now;

      checkConsumerPurchase();
    }


    // =================================================
    // POST PRODUCER METRICS
    // =================================================

    if (now - lastMetricsPost >=
        METRICS_INTERVAL) {

      lastMetricsPost = now;


      float current = 0.0;


      // Get latest sensor value
      if (xSemaphoreTake(
            stateMutex,
            pdMS_TO_TICKS(10)
          ) == pdTRUE) {

        current = latestCurrent;

        xSemaphoreGive(stateMutex);
      }


      postProducerMetrics(current);
    }


    // =================================================
    // GIVE CPU TO OTHER TASKS
    // =================================================

    vTaskDelay(
      pdMS_TO_TICKS(NETWORK_TASK_DELAY_MS)
    );
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

  for (
    int i = 0;
    i < CALIBRATION_SAMPLES;
    i++
  ) {

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

  stateMutex =
    xSemaphoreCreateMutex();

  if (stateMutex == NULL) {

    Serial.println(
      "ERROR: Failed to create mutex!"
    );

    while (true) {

      delay(1000);
    }
  }


  // ===================================================
  // INITIALIZE SHARED STATE
  // ===================================================

  latestCurrent = 0.0;

  currentIsOn = false;

  relayTrigger = false;

  relayActive = false;

  producerEnergyKWh = 0.0;


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
  Serial.println("--------------------------------");
  Serial.println("Producer metrics enabled");
  Serial.println("Endpoint:");
  Serial.println(METRICS_URL);
  Serial.println("--------------------------------");
  Serial.println("Producer voltage = 5.00 V");
  Serial.println("Power factor = 0.95");
  Serial.println("================================");
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  // All work is handled by FreeRTOS tasks.

  vTaskDelay(
    pdMS_TO_TICKS(1000)
  );
}