#define ACS712_PIN 34

// ACS712 5A sensitivity
const float SENSITIVITY = 0.185;  // 185 mV/A

float zeroVoltage = 0;

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(ACS712_PIN, ADC_11db);

  Serial.println("Calibrating zero current...");
  Serial.println("Make sure NO current is flowing.");

  delay(2000);

  long total = 0;

  for (int i = 0; i < 2000; i++) {
    total += analogRead(ACS712_PIN);
    delay(1);
  }

  float averageADC = total / 2000.0;

  zeroVoltage = (averageADC / 4095.0) * 3.3;

  Serial.print("Zero ADC = ");
  Serial.println(averageADC);

  Serial.print("Zero Voltage = ");
  Serial.print(zeroVoltage, 4);
  Serial.println(" V");

  Serial.println("Calibration complete.");
}

void loop() {

  long total = 0;

  // Take many samples
  for (int i = 0; i < 1000; i++) {
    total += analogRead(ACS712_PIN);
  }

  float averageADC = total / 1000.0;

  float voltage = (averageADC / 4095.0) * 3.3;

  float current = (voltage - zeroVoltage) / SENSITIVITY;

  // Remove small noise around zero
  if (current > -0.10 && current < 0.10) {
    current = 0;
  }
  current = -1*current;
  Serial.print("Current = ");
  Serial.print(current, 3);
  Serial.println(" A");

  delay(500);
}