#include <WiFi.h>
#include <math.h>

// ==================================================
// Wi-Fi Credentials
// ==================================================

const char* ssid     = "Oppo";
const char* password = "7356125156";


// ==================================================
// Pins
// ==================================================

#define ACS_PIN         34
#define CIRC_STATE_PIN  32


// ==================================================
// Energy Meter Configuration
// ==================================================

// ACS712 5A sensitivity ≈ 185 mV/A
const float SENSITIVITY = 0.185;   // V/A

// Supply voltage
const float SUPPLY_VOLTAGE = 9.0;  // V

// Electricity rate
const float RATE_PER_KWH = 7.5;


// ==================================================
// State Variables
// ==================================================

float zeroVoltage = 0.0;

float totalWh = 0.0;

unsigned long previousTime = 0;


// ==================================================
// Setup
// ==================================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12);   // 0–4095

  pinMode(CIRC_STATE_PIN, INPUT);


  // =================================================
  // ACS712 Calibration
  // =================================================

  Serial.println();
  Serial.println("======================================");
  Serial.println("     ACS712 CALIBRATION"
  Serial.println("======================================");

  Serial.println("Make sure NO current is flowing...");

  long total = 0;

  for (int i = 0; i < 1000; i++) {

    total += analogRead(ACS_PIN);

    delay(1);
  }

  float averageADC = total / 1000.0;

  zeroVoltage =
      (averageADC / 4095.0) * 3.3;


  Serial.print("Zero ADC: ");
  Serial.println(averageADC, 2);

  Serial.print("Zero voltage: ");
  Serial.print(zeroVoltage, 4);
  Serial.println(" V");


  // =================================================
  // Wi-Fi Connection
  // =================================================

  Serial.println();
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println();

  Serial.println("Wi-Fi Connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());


  // =================================================
  // Start Energy Measurement
  // =================================================

  Serial.println();
  Serial.println("======================================");
  Serial.println("   VoltShare Real Energy Meter");
  Serial.println("======================================");

  previousTime = millis();
}


// ==================================================
// Loop
// ==================================================

void loop() {


  // =================================================
  // Read ACS712
  // =================================================

  long total = 0;

  const int samples = 100;

  for (int i = 0; i < samples; i++) {

    total += analogRead(ACS_PIN);
  }

  float averageADC =
      total / (float)samples;


  // =================================================
  // Convert ADC → Voltage
  // =================================================

  float sensorVoltage =
      (averageADC / 4095.0) * 3.3;


  // =================================================
  // Convert Voltage → Current
  // =================================================

  float currentAmps =
      (sensorVoltage - zeroVoltage) / SENSITIVITY;


  // Reverse sensor direction if required
  currentAmps = -currentAmps;


  // =================================================
  // Remove Small Noise
  // =================================================

  if (fabs(currentAmps) < 0.05) {

    currentAmps = 0.0;
  }


  // =================================================
  // Current in mA
  // =================================================

  float currentmA =
      currentAmps * 1000.0;


  // =================================================
  // Use Absolute Current for Consumption
  // =================================================

  float consumptionAmps =
      fabs(currentAmps);


  // =================================================
  // Read Circuit State
  // =================================================

  int lightValue =
      analogRead(CIRC_STATE_PIN);


  // =================================================
  // Calculate Elapsed Time
  // =================================================

  unsigned long currentTime =
      millis();

  float timeSeconds =
      (currentTime - previousTime) / 1000.0;

  previousTime = currentTime;


  float timeHours =
      timeSeconds / 3600.0;


  // =================================================
  // Calculate Power
  // =================================================

  float watts =
      SUPPLY_VOLTAGE * consumptionAmps;


  // =================================================
  // Calculate Energy
  // =================================================

  float energyWh =
      watts * timeHours;

  totalWh += energyWh;


  // =================================================
  // Calculate kWh
  // =================================================

  float energyKWh =
      totalWh / 1000.0;


  // =================================================
  // Calculate Cost
  // =================================================

  float cost =
      energyKWh * RATE_PER_KWH;


  // =================================================
  // Serial Monitor
  // =================================================

  Serial.println("--------------------------------------");

  Serial.print("ADC: ");
  Serial.println(averageADC, 2);

  Serial.print("Sensor Voltage: ");
  Serial.print(sensorVoltage, 4);
  Serial.println(" V");

  Serial.print("Voltage: ");
  Serial.print(SUPPLY_VOLTAGE, 2);
  Serial.println(" V");

  Serial.print("Current: ");
  Serial.print(currentmA, 3);
  Serial.println(" mA");

  Serial.print("Power: ");
  Serial.print(watts, 3);
  Serial.println(" W");

  Serial.print("Energy: ");
  Serial.print(totalWh, 6);
  Serial.println(" Wh");

  Serial.print("Energy: ");
  Serial.print(energyKWh, 9);
  Serial.println(" kWh");

  Serial.print("Cost: ");
  Serial.print(cost, 6);
  Serial.println();

  Serial.print("CIRC_STATE: ");
  Serial.println(lightValue);


  // =================================================
  // Wi-Fi Check
  // =================================================

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("Wi-Fi disconnected. Reconnecting...");

    WiFi.disconnect();

    WiFi.begin(ssid, password);

    delay(500);

    return;
  }


  // =================================================
  // Measurement Interval
  // =================================================

  delay(500);
}