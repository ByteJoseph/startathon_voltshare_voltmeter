#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#define ACS712_PIN 34
#define RELAY_PIN 32

// ===============================
// Wi-Fi credentials
// ===============================
const char* WIFI_SSID = "Oppo";
const char* WIFI_PASSWORD = "7356125156";
const char* SERVER_URL = "https://startathon-voltshare-smartmeter.onrender.com";

const float SENSITIVITY = 0.185;
const float ADC_REFERENCE = 3.3;
const float ADC_MAX = 4095.0;
const float CURRENT_NOISE_THRESHOLD = 0.10;

float zeroVoltage = 0.0;
String response;

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

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  Serial.print("Sending request: ");
  Serial.println(url);

  // HTTPS connection
  if (!http.begin(client, url)) {

    Serial.println("HTTP begin failed.");
    return "";
  }

  http.setConnectTimeout(10000);
  http.setTimeout(10000);

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

void setup() {
  // Back to standard output mode
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  delay(1000);

  connectWiFi();

  analogReadResolution(12);

  analogSetPinAttenuation(
    ACS712_PIN,
    ADC_11db
  );

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
}

void loop() {
  // // Transistor ON -> Relay ON -> LED turns ON, Motor turns OFF
  // digitalWrite(RELAY_PIN, HIGH);
  // delay(1000);

  // // Transistor OFF -> Relay OFF -> Motor turns ON, LED turns OFF
  // digitalWrite(RELAY_PIN, LOW);
  // delay(1000);

  long total = 0;

  for (int i = 0; i < 1000; i++) {

    total += analogRead(ACS712_PIN);

    delayMicroseconds(100);
  }

  float averageADC =
    total / 1000.0;

  float voltage =
    (averageADC / ADC_MAX) * ADC_REFERENCE;

  float current =
    (voltage - zeroVoltage) / SENSITIVITY;


  // -------------------------------
  // Remove small noise
  // -------------------------------
  if (current > -CURRENT_NOISE_THRESHOLD &&
      current < CURRENT_NOISE_THRESHOLD) {

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
  if (fabs(current) < CURRENT_NOISE_THRESHOLD) {

    current = 0;

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

  delay(50);
}