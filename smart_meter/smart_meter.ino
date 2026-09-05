#define ACS_PIN 34

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);  // 0–4095
}

void loop() {
  int adcValue = analogRead(ACS_PIN);

  Serial.print("ACS ADC: ");
  Serial.println(adcValue);

  delay(500);
}