#define RELAY_PIN 32

void setup() {
  // Back to standard output mode
  pinMode(RELAY_PIN, OUTPUT); 
}

void loop() {
  // Transistor ON -> Relay ON -> LED turns ON, Motor turns OFF
  digitalWrite(RELAY_PIN, HIGH);
  delay(1000);

  // Transistor OFF -> Relay OFF -> Motor turns ON, LED turns OFF
  digitalWrite(RELAY_PIN, LOW);
  delay(1000);
}