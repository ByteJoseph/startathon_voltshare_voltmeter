#include <WiFi.h>
#include <HTTPClient.h>

#define ACS712_PIN 34

// ===============================
// Wi-Fi credentials
// ===============================
const char* WIFI_SSID = "Oppo";
const char* WIFI_PASSWORD = "7356125156";

// ===============================
// Server
// ===============================
const char* SERVER_URL =
  "https://startathon-voltshare-smartmeter.onrender.com/button/status";

// ===============================
// ACS712 5A
// Sensitivity = 185 mV/A
// ===============================
const float SENSITIVITY = 0.185;

float zeroVoltage = 0.0;
String response;


// ===============================
// Connect to Wi-Fi
// ===============================
void connectWiFi() {

  Serial.println();
  Serial.println("==============================");
  Serial.println("Starting WiFi connection...");
  Serial.print("WiFi name: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 40) {

    delay(500);
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
    Serial.println("Check WiFi name and password.");
  }

  Serial.println("==============================");
}


// ===============================
// HTTP GET
// ===============================
String httpGET(const String& url) {

  // Check WiFi
  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi disconnected.");
    Serial.println("Trying to reconnect...");

    connectWiFi();

    if (WiFi.status() != WL_CONNECTED) {

      Serial.println("Could not reconnect to WiFi.");
      return "";
    }
  }

  HTTPClient http;

  Serial.print("Sending request: ");
  Serial.println(url);

  http.begin(url);

  int httpCode = http.GET();

  if (httpCode > 0) {

    Serial.print("HTTP Response Code: ");
    Serial.println(httpCode);

    String result = http.getString();

    Serial.print("Server response: ");
    Serial.println(result);

    http.end();

    return result;
  }

  Serial.print("HTTP GET failed: ");
  Serial.println(http.errorToString(httpCode));

  http.end();

  return "";
}


// ===============================
// SETUP
// ===============================
void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 SMART METER");
  Serial.println("PROGRAM STARTED");
  Serial.println("================================");

  // -------------------------------
  // WiFi
  // -------------------------------
  connectWiFi();

  // -------------------------------
  // ADC configuration
  // -------------------------------
  analogReadResolution(12);

  analogSetPinAttenuation(
    ACS712_PIN,
    ADC_11db
  );

  // -------------------------------
  // ACS712 calibration
  // -------------------------------
  Serial.println();
  Serial.println("Calibrating zero current...");
  Serial.println("Make sure NO current is flowing.");

  delay(2000);

  long total = 0;

  for (int i = 0; i < 2000; i++) {

    total += analogRead(ACS712_PIN);

    delay(1);
  }

  float averageADC =
    total / 2000.0;

  zeroVoltage =
    (averageADC / 4095.0) * 3.3;

  Serial.print("Zero ADC = ");
  Serial.println(averageADC);

  Serial.print("Zero Voltage = ");
  Serial.print(zeroVoltage, 4);
  Serial.println(" V");

  Serial.println("Calibration complete.");

  Serial.println();
  Serial.println("ESP32 is ready.");
  Serial.println("================================");
}


// ===============================
// LOOP
// ===============================
void loop() {

  // -------------------------------
  // Read ACS712
  // -------------------------------
  long total = 0;

  for (int i = 0; i < 1000; i++) {

    total += analogRead(ACS712_PIN);
  }

  float averageADC =
    total / 1000.0;

  float voltage =
    (averageADC / 4095.0) * 3.3;

  float current =
    (voltage - zeroVoltage) / SENSITIVITY;


  // -------------------------------
  // Remove small noise
  // -------------------------------
  if (current > -0.10 && current < 0.10) {

    current = 0;
  }


  // -------------------------------
  // Display current
  // -------------------------------
  float displayCurrent = -1 * current;
  current = displayCurrent;

  Serial.println();
  Serial.println("------------------------------");

  Serial.print("ADC = ");
  Serial.println(averageADC);

  Serial.print("Voltage = ");
  Serial.print(voltage, 4);
  Serial.println(" V");

  Serial.print("Current = ");
  Serial.print(displayCurrent, 3);
  Serial.println(" A");


  // -------------------------------
  // Send ON/OFF status
  // -------------------------------
  if (current == 0) {

    Serial.println("STATUS: OFF");
    Serial.println("Sending OFF request...");

    response = httpGET(
      String(SERVER_URL) + "?off=true"
    );

  } else {

    Serial.println("STATUS: ON");
    Serial.println("Sending ON request...");

    response = httpGET(
      String(SERVER_URL) + "?on=true"
    );
  }


  Serial.println("------------------------------");

  delay(500);
}