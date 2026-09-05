#define ACS_PIN 34
#define CIRC_STATE_PIN 32
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);  // 0–4095
}

void loop() {
  int adcValue = analogRead(ACS_PIN);
  int lightValue = analogRead(CIRC_STATE_PIN);
  Serial.print("ACS ADC: ");
  Serial.println(adcValue);
  Serial.print("LIGHT: ");
  Serial.println(lightValue);
  delay(500);
}