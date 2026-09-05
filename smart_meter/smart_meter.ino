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

const char* SERVER_URL =
  "https://startathon-voltshare-smartmeter.onrender.com";

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
// SPEED / TIMING CONFIGURATION
// =====================================================

// Number of ADC samples per measurement
const int ADC_SAMPLES = 200;

// Delay between ADC samples
const int ADC_SAMPLE_DELAY_US = 100;

// Check server for purchase every 1 second
const unsigned long PURCHASE_INTERVAL = 1000;

// Send ON/OFF status at most every 2 seconds
const unsigned long STATUS_INTERVAL = 2000;

// Relay ON duration
const unsigned long RELAY_DURATION = 2000;


// =====================================================
// GLOBAL VARIABLES
// =====================================================

float zeroVoltage = 0.0;

String response;

// Timers
unsigned long lastPurchaseCheck = 0;
unsigned long lastStatusSend = 0;
unsigned long relayStartTime = 0;

// Relay state
bool relayActive = false;

// Current state
bool currentIsOn = false;
bool lastSentState = false;
bool firstStatusSend = true;


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

    delay(250);
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

    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi reconnected!");
    return true;
  }

  Serial.println("WiFi reconnect failed.");

  return false;
}


// =====================================================
// HTTP GET
// =====================================================

String httpGET(const String& url) {

  if (!ensureWiFi()) {
    return "";
  }

  WiFiClientSecure client;

  // Skip certificate verification
  // Good for testing, but not recommended for production.
  client.setInsecure();

  HTTPClient http;

  Serial.print("GET: ");
  Serial.println(url);

  if (!http.begin(client, url)) {

    Serial.println("HTTP begin failed.");
    return "";
  }

  // Shorter timeout so ESP32 doesn't get stuck for 10 seconds
  http.setConnectTimeout(3000);
  http.setTimeout(3000);

  int httpCode = http.GET();

  if (httpCode > 0) {

    Serial.print("HTTP code: ");
    Serial.println(httpCode);

    String result = http.getString();

    http.end();

    return result;
  }

  Serial.print("HTTP GET failed: ");
  Serial.println(http.errorToString(httpCode));

  http.end();

  return "";
}


// =====================================================
// SEND ON/OFF STATUS
// =====================================================

void sendStatus(bool isOn) {

  if (!ensureWiFi()) {
    return;
  }

  String url;

  if (isOn) {

    url = String(SERVER_URL) + "?on=true";

    Serial.println();
    Serial.println("Sending ON status...");

  } else {

    url = String(SERVER_URL) + "?off=true";

    Serial.println();
    Serial.println("Sending OFF status...");
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  if (!http.begin(client, url)) {

    Serial.println("Failed to start status request.");
    return;
  }

  http.setConnectTimeout(3000);
  http.setTimeout(3000);

  int httpCode = http.GET();

  Serial.print("Status HTTP code: ");
  Serial.println(httpCode);

  // We don't actually need the response body here.
  // Just close the connection.
  http.end();
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
  // ACTIVATE RELAY
  // ===================================================

  Serial.println("================================");
  Serial.println("PURCHASE DETECTED!");
  Serial.println("Relay ON");
  Serial.println("================================");

  digitalWrite(RELAY_PIN, HIGH);

  relayActive = true;

  relayStartTime = millis();
}


// =====================================================
// HANDLE RELAY TIMER
// =====================================================

void handleRelay() {

  if (!relayActive) {
    return;
  }

  if (millis() - relayStartTime >= RELAY_DURATION) {

    digitalWrite(RELAY_PIN, LOW);

    relayActive = false;

    Serial.println("Relay OFF");
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


  // Your original code reverses the sign
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


  Serial.println();
  Serial.println("ESP32 is ready.");
  Serial.println("================================");


  // Allow immediate purchase check
  lastPurchaseCheck = millis() - PURCHASE_INTERVAL;

  // Allow immediate status update
  lastStatusSend = millis() - STATUS_INTERVAL;
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  unsigned long now = millis();


  // ===================================================
  // 1. HANDLE RELAY WITHOUT BLOCKING
  // ===================================================

  handleRelay();


  // ===================================================
  // 2. CHECK CONSUMER PURCHASE EVERY 1 SECOND
  // ===================================================

  if (now - lastPurchaseCheck >= PURCHASE_INTERVAL) {

    lastPurchaseCheck = now;

    checkConsumerPurchase();
  }


  // ===================================================
  // 3. MEASURE CURRENT
  // ===================================================

  float current = readCurrent();


  // ===================================================
  // 4. DETERMINE ON/OFF STATE
  // ===================================================

  bool newCurrentState =
    fabs(current) >= CURRENT_NOISE_THRESHOLD;


  if (newCurrentState) {

    Serial.println("STATUS: ON");

  } else {

    Serial.println("STATUS: OFF");
  }


  // ===================================================
  // 5. SEND STATUS ONLY WHEN NECESSARY
  // ===================================================

  bool stateChanged =
    newCurrentState != lastSentState;


  bool periodicUpdate =
    now - lastStatusSend >= STATUS_INTERVAL;


  if (firstStatusSend ||
      stateChanged ||
      periodicUpdate) {

    sendStatus(newCurrentState);

    lastSentState = newCurrentState;

    lastStatusSend = millis();

    firstStatusSend = false;
  }


  // ===================================================
  // VERY SMALL DELAY
  // ===================================================

  delay(5);
}