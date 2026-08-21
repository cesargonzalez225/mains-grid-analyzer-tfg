
#define SENSE_PIN A0
#define ADC_VREF 5.0
#define ADC_MAX_COUNT 1023.0

#define SAMPLE_INTERVAL_US 200
#define MAX_CYCLE_SAMPLES 150
#define CROSSING_HYSTERESIS_V 0.02
#define CAL_DURATION_US 1000000
#define PRINT_INTERVAL_MS 500

float expectedRms = 230.0;

float crossing = ADC_VREF / 2.0;
float conversionRate = 1.0;

float cycleBuffer[MAX_CYCLE_SAMPLES];
int cycleSampleCount = 0;
bool wasBelowCrossing = true;
bool haveLastCrossing = false;

uint32_t lastSampleUs = 0;
float prevVoltage = 0.0;
uint32_t prevSampleUs = 0;
float lastInterpCrossingUs = 0.0;

float lastVrms = -1.0;
float lastFreqHz = -1.0;
uint32_t lastPrintMs = 0;

float readVoltage() {
  int raw = analogRead(SENSE_PIN);
  return (raw * ADC_VREF) / ADC_MAX_COUNT;
}

void runCalibration() {
  Serial.print(F("Calibrating against expected "));
  Serial.print(expectedRms, 1);
  Serial.println(F("V RMS - make sure the sensor is reading nominal mains right now."));

  Serial.println(F("  pass 1/2: finding crossing point..."));
  float obsMin = ADC_VREF;
  float obsMax = 0.0;
  {
    uint32_t start = micros();
    uint32_t nextSample = start;
    while (micros() - start < CAL_DURATION_US) {
      uint32_t now = micros();
      if (now - nextSample < SAMPLE_INTERVAL_US) continue;
      nextSample = now;

      float v = readVoltage();
      if (v < obsMin) obsMin = v;
      if (v > obsMax) obsMax = v;
    }
  }
  float newCrossing = (obsMin + obsMax) / 2.0;

  Serial.println(F("  pass 2/2: measuring RMS..."));
  double sumSq = 0.0;
  uint32_t count = 0;
  {
    uint32_t start = micros();
    uint32_t nextSample = start;
    while (micros() - start < CAL_DURATION_US) {
      uint32_t now = micros();
      if (now - nextSample < SAMPLE_INTERVAL_US) continue;
      nextSample = now;

      float v = readVoltage();
      double ac = v - newCrossing;
      sumSq += ac * ac;
      count++;
    }
  }
  float observedRms = (count > 0) ? (float)sqrt(sumSq / count) : 0.0;

  if (observedRms < 0.02) {
    Serial.println(F("Calibration FAILED: observed signal under 20mV RMS - check wiring/signal. Keeping previous calibration."));
    return;
  }

  crossing = newCrossing;
  conversionRate = expectedRms / observedRms;

  Serial.print(F("Calibrated: observed ADC range ")); Serial.print(obsMin, 3);
  Serial.print(F("V - ")); Serial.print(obsMax, 3); Serial.println(F("V"));
  Serial.print(F("  crossing = ")); Serial.print(crossing, 3); Serial.println(F(" V"));
  Serial.print(F("  observed RMS = ")); Serial.print(observedRms, 4); Serial.println(F(" V"));
  Serial.print(F("  conversion_rate = ")); Serial.println(conversionRate, 3);
}

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (line.equalsIgnoreCase("CAL")) {
    runCalibration();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Nano Vrms/frequency test"));
  Serial.println(F("Send CAL over the serial monitor (while the sensor reads nominal mains) to scale readings to real volts."));
  lastSampleUs = micros();
  prevSampleUs = lastSampleUs;
  prevVoltage = readVoltage();
}

void loop() {
  handleSerial();

  uint32_t now = micros();
  if (now - lastSampleUs < SAMPLE_INTERVAL_US) return;
  lastSampleUs = now;

  float voltage = readVoltage();

  if (cycleSampleCount < MAX_CYCLE_SAMPLES) {
    cycleBuffer[cycleSampleCount++] = voltage;
  }

  bool isBelow = voltage < (crossing - CROSSING_HYSTERESIS_V);
  bool isAbove = voltage > (crossing + CROSSING_HYSTERESIS_V);

  if (wasBelowCrossing && isAbove) {

    float frac = (crossing - prevVoltage) / (voltage - prevVoltage);
    float interpUs = prevSampleUs + frac * (now - prevSampleUs);

    if (haveLastCrossing) {
      float periodUs = interpUs - lastInterpCrossingUs;
      if (periodUs > 0) {
        float freq = 1000000.0 / periodUs;

        double sum = 0.0;
        for (int i = 0; i < cycleSampleCount; i++) sum += cycleBuffer[i];
        double dc = (cycleSampleCount > 0) ? (sum / cycleSampleCount) : crossing;

        double sumSq = 0.0;
        for (int i = 0; i < cycleSampleCount; i++) {
          double ac = cycleBuffer[i] - dc;
          sumSq += ac * ac;
        }
        float rawRms = (cycleSampleCount > 0) ? sqrt(sumSq / cycleSampleCount) : 0.0;

        lastVrms = rawRms * conversionRate;
        lastFreqHz = freq;
      }
    }

    lastInterpCrossingUs = interpUs;
    haveLastCrossing = true;
    cycleSampleCount = 0;
  }

  if (isBelow) wasBelowCrossing = true;
  else if (isAbove) wasBelowCrossing = false;

  prevVoltage = voltage;
  prevSampleUs = now;

  uint32_t nowMs = millis();
  if (nowMs - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = nowMs;
    if (lastVrms >= 0) {
      Serial.print(F("Vrms:")); Serial.print(lastVrms, 1);
      Serial.print(F("  freq:")); Serial.print(lastFreqHz, 2);
      Serial.println(F("Hz"));
    } else {
      Serial.println(F("waiting for first full cycle..."));
    }
  }
}
