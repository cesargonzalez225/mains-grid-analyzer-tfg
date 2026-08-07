
#define SENSE_PIN A0
#define ADC_MAX_COUNT 1023.0
#define ADC_VREF 5.0
#define PRINT_INTERVAL_MS 100

float minSeen = ADC_VREF;
float maxSeen = 0.0;
uint32_t lastPrintMs = 0;

float readVoltage() {
  int raw = analogRead(SENSE_PIN);
  return (raw * ADC_VREF) / ADC_MAX_COUNT;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println(F("Nano voltage probe - reading A0"));
  Serial.println(F("Send R to reset min/max"));
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'R' || c == 'r') {
      minSeen = ADC_VREF;
      maxSeen = 0.0;
      Serial.println(F("min/max reset"));
    }
  }

  float v = readVoltage();
  if (v < minSeen) minSeen = v;
  if (v > maxSeen) maxSeen = v;

  uint32_t now = millis();
  if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = now;
    Serial.print(F("V:")); Serial.print(v, 3);
    Serial.print(F("  min:")); Serial.print(minSeen, 3);
    Serial.print(F("  max:")); Serial.println(maxSeen, 3);
  }
}
