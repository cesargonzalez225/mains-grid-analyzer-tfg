
#include <RadioLib.h>
#include <stdlib.h>

#define LORA_NSS   17
#define LORA_DIO1  20
#define LORA_NRST  22
#define LORA_BUSY  21
#define LORA_RXEN  15
#define LORA_TXEN  14

#define LORA_FREQ_MHZ   915.0
#define LORA_BW_KHZ     125.0
#define LORA_SF         9
#define LORA_CR         7
#define LORA_SYNC_WORD  0x12
#define LORA_POWER_DBM  17

#define LORA_TX_TIMEOUT_MS 2000

#define SENSE_PIN A0

#define ADC_BITS       12
#define ADC_MAX_COUNT  ((1 << ADC_BITS) - 1)
#define ADC_VREF       3.3

#define SAMPLE_INTERVAL_US 200
#define WAVEFORM_DECIMATION 5

#define CAL_DURATION_US    1000000
#define MAX_CAL_SAMPLES    5000

#define CROSSING_DRIFT_ALPHA 0.05

#define SAG_THRESHOLD_PCT   90.0
#define SWELL_THRESHOLD_PCT 110.0

#define MAX_CYCLE_SAMPLES 200

float expectedRms = 230.0;
float expectedHz = 50.0;

float crossing = 1.65;
float conversionRate = 1.0;

bool waveformMode = false;
uint8_t waveformSampleCounter = 0;

float cycleMin = ADC_VREF;
float cycleMax = 0.0;
bool wasBelowCrossing = true;
bool haveLastCrossing = false;
uint32_t lastSampleUs = 0;

float prevVoltage = 0.0;
uint32_t prevSampleUs = 0;
float lastInterpCrossingUs = 0.0;

float cycleBuffer[MAX_CYCLE_SAMPLES];
int cycleSampleCount = 0;

float calSamples[MAX_CAL_SAMPLES];

enum PQState { PQ_NORMAL, PQ_SAG, PQ_SWELL };
PQState pqState = PQ_NORMAL;
uint32_t pqEventStartMs = 0;

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);
bool radioOk = false;
uint32_t radioCounter = 0;
uint32_t lastLoraTxMs = 0;

#define MIN_LORA_INTERVAL_MS 500

float readVoltage() {
  int raw = analogRead(SENSE_PIN);
  return (raw * ADC_VREF) / ADC_MAX_COUNT;
}

int reliableTransmit(String &packet) {
  int state = radio.startTransmit(packet);
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }

  uint32_t deadline = millis() + LORA_TX_TIMEOUT_MS;
  while (digitalRead(LORA_DIO1) == LOW) {
    if (millis() > deadline) {
      radio.finishTransmit();
      return RADIOLIB_ERR_TX_TIMEOUT;
    }
  }

  return radio.finishTransmit();
}

void sendWarning(uint8_t code, const String &text) {
  if (!radioOk) return;

  uint32_t now = millis();
  if (now - lastLoraTxMs < MIN_LORA_INTERVAL_MS) {
    Serial.println(F("LoRa: skipped (rate limit - previous send was too recent)"));
    return;
  }
  lastLoraTxMs = now;

  radioCounter++;
  String packet = "W|" + String(radioCounter) + "|" + String(code) + "|" + text;

  Serial.print(F("LoRa sending: "));
  Serial.println(packet);

  uint32_t txStart = millis();
  int state = reliableTransmit(packet);
  uint32_t txMs = millis() - txStart;

  Serial.print(F("  LoRa transmit took ")); Serial.print(txMs); Serial.println(F(" ms"));
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("  LoRa transmit failed, code "));
    Serial.println(state);
  }
}

float goertzelMagnitude(float *samples, int n, int k) {
  if (n < 8) return 0.0;
  float w = 2.0 * PI * k / n;
  float coeff = 2.0 * cos(w);
  float q1 = 0.0, q2 = 0.0;
  for (int i = 0; i < n; i++) {
    float windowed = samples[i] * (0.5 - 0.5 * cos(2.0 * PI * i / (n - 1)));
    float q0 = coeff * q1 - q2 + windowed;
    q2 = q1;
    q1 = q0;
  }
  float real = q1 - q2 * cos(w);
  float imag = q2 * sin(w);
  return sqrt(real * real + imag * imag) / (n / 2.0);
}

void resetCycleTracking() {
  cycleMin = ADC_VREF;
  cycleMax = 0.0;
  cycleSampleCount = 0;
  haveLastCrossing = false;
}

int compareFloats(const void *a, const void *b) {
  float fa = *(const float *)a;
  float fb = *(const float *)b;
  if (fa < fb) return -1;
  if (fa > fb) return 1;
  return 0;
}

void runCalibration() {
  Serial.print(F("Calibrating against expected "));
  Serial.print(expectedRms, 1);
  Serial.print(F("V RMS / "));
  Serial.print(expectedHz, 1);
  Serial.println(F("Hz - observing signal for 1s..."));

  int count = 0;
  uint32_t start = micros();
  uint32_t nextSample = start;

  while (micros() - start < CAL_DURATION_US) {
    uint32_t now = micros();
    if (now - nextSample < SAMPLE_INTERVAL_US) continue;
    nextSample = now;

    if (count < MAX_CAL_SAMPLES) {
      calSamples[count++] = readVoltage();
    }
  }

  if (count < 100) {
    Serial.println(F("Calibration FAILED: not enough samples collected. Keeping previous calibration."));
    return;
  }

  qsort(calSamples, count, sizeof(float), compareFloats);
  int trim = count / 100;
  float obsMin = calSamples[trim];
  float obsMax = calSamples[count - 1 - trim];

  float observedVpp = obsMax - obsMin;
  if (observedVpp < 0.05) {
    Serial.println(F("Calibration FAILED: observed swing under 50mV - check wiring/signal. Keeping previous calibration."));
    return;
  }

  crossing = (obsMin + obsMax) / 2.0;
  float expectedVpp = expectedRms * 1.41421356 * 2.0;
  conversionRate = expectedVpp / observedVpp;

  resetCycleTracking();
  pqState = PQ_NORMAL;

  Serial.print(F("Calibrated: observed ADC range ")); Serial.print(obsMin, 3);
  Serial.print(F("V - ")); Serial.print(obsMax, 3); Serial.println(F("V"));
  Serial.print(F("  crossing = ")); Serial.print(crossing, 3); Serial.println(F(" V"));
  Serial.print(F("  conversion_rate = ")); Serial.println(conversionRate, 3);
}

void printStatus() {
  Serial.println(F("--- calibration status ---"));
  Serial.print(F("expected target: ")); Serial.print(expectedRms, 1);
  Serial.print(F("V RMS, ")); Serial.print(expectedHz, 1); Serial.println(F("Hz"));
  Serial.print(F("crossing:        ")); Serial.print(crossing, 3); Serial.println(F(" V"));
  Serial.print(F("conversion_rate: ")); Serial.println(conversionRate, 3);
  Serial.println(F("--------------------------"));
}

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (line.equalsIgnoreCase("CAL")) {
    runCalibration();
    return;
  }
  if (line == "?") {
    printStatus();
    return;
  }
  if (line.equalsIgnoreCase("W")) {
    waveformMode = true;
    waveformSampleCounter = 0;
    Serial.println(F("waveform stream on - signal: lines at ~1kHz alongside measurement"));
    return;
  }
  if (line.equalsIgnoreCase("M")) {
    waveformMode = false;
    Serial.println(F("waveform stream off"));
    return;
  }
  if (line.equalsIgnoreCase("T")) {

    if (!radioOk) {
      Serial.println(F("can't test - radio didn't initialize at boot (check module power/wiring, then reset the Pico)"));
    } else {
      sendWarning(1, "Test message from Pico");
    }
    return;
  }

  char cmd = line.charAt(0);
  float value = line.substring(1).toFloat();

  switch (cmd) {
    case 'V': case 'v':
      expectedRms = value;
      Serial.print(F("expected RMS voltage set to ")); Serial.println(expectedRms, 1);
      break;
    case 'H': case 'h':
      expectedHz = value;
      Serial.print(F("expected frequency set to ")); Serial.println(expectedHz, 1);
      break;
    default:
      Serial.println(F("unknown command. Use V<rms>, H<hz>, CAL, W, M, or ?"));
      break;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  analogReadResolution(ADC_BITS);

  Serial.println(F("Pico mains analyzer - voltage, frequency, sag/swell, harmonics"));
  Serial.println(F("Send V<rms>/H<hz> to change the expected target, or CAL to recalibrate anytime."));

  radio.setRfSwitchPins(LORA_RXEN, LORA_TXEN);
  Serial.print(F("[SX1262] Initializing ... "));

  int radioState = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR, LORA_SYNC_WORD, LORA_POWER_DBM, 8, 0, true);
  if (radioState == RADIOLIB_ERR_NONE) {
    radioOk = true;
    Serial.println(F("success! LoRa warnings enabled."));
  } else {
    radioOk = false;
    Serial.print(F("failed, code "));
    Serial.print(radioState);
    Serial.println(F(" - continuing WITHOUT LoRa (sensing still works standalone). Check module power/wiring and send nothing to retry - it only inits once at boot."));
  }

  runCalibration();
}

void loop() {
  handleSerial();

  uint32_t now = micros();
  if (now - lastSampleUs < SAMPLE_INTERVAL_US) return;
  lastSampleUs = now;

  float voltage = readVoltage();

  if (waveformMode) {
    waveformSampleCounter++;
    if (waveformSampleCounter >= WAVEFORM_DECIMATION) {
      waveformSampleCounter = 0;
      Serial.print(F("signal:"));
      Serial.println(voltage, 3);
    }
  }

  if (voltage < cycleMin) cycleMin = voltage;
  if (voltage > cycleMax) cycleMax = voltage;
  if (cycleSampleCount < MAX_CYCLE_SAMPLES) {
    cycleBuffer[cycleSampleCount++] = voltage;
  }

  bool isBelowCrossing = voltage < crossing;

  if (wasBelowCrossing && !isBelowCrossing) {

    float denom = voltage - prevVoltage;
    float frac = (denom != 0.0) ? (crossing - prevVoltage) / denom : 0.0;
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;
    float interpCrossingUs = prevSampleUs + frac * (float)(now - prevSampleUs);

    if (haveLastCrossing) {
      float periodUs = interpCrossingUs - lastInterpCrossingUs;
      float frequency = 1000000.0 / periodUs;

      float measuredVpp = cycleMax - cycleMin;
      float realVpp = measuredVpp * conversionRate;
      float realPeak = realVpp / 2.0;
      float realRms = realPeak / 1.41421356;

      float magFund = goertzelMagnitude(cycleBuffer, cycleSampleCount, 1);
      float mag3 = goertzelMagnitude(cycleBuffer, cycleSampleCount, 3);
      float mag5 = goertzelMagnitude(cycleBuffer, cycleSampleCount, 5);
      float mag7 = goertzelMagnitude(cycleBuffer, cycleSampleCount, 7);
      float thdPct = 0.0, h3Pct = 0.0, h5Pct = 0.0;
      if (magFund > 0.005) {
        thdPct = 100.0 * sqrt(mag3 * mag3 + mag5 * mag5 + mag7 * mag7) / magFund;
        h3Pct = 100.0 * mag3 / magFund;
        h5Pct = 100.0 * mag5 / magFund;
      }

      Serial.print(F("freq:")); Serial.print(frequency, 2);
      Serial.print(F(" Vpeak:")); Serial.print(realPeak, 1);
      Serial.print(F(" Vrms:")); Serial.print(realRms, 1);
      Serial.print(F(" THD:")); Serial.print(thdPct, 2);
      Serial.print(F(" H3pct:")); Serial.print(h3Pct, 2);
      Serial.print(F(" H5pct:")); Serial.print(h5Pct, 2);
      Serial.print(F(" crossing:")); Serial.println(crossing, 4);

      float pctOfNominal = (realRms / expectedRms) * 100.0;
      PQState newState;
      if (pctOfNominal < SAG_THRESHOLD_PCT) newState = PQ_SAG;
      else if (pctOfNominal > SWELL_THRESHOLD_PCT) newState = PQ_SWELL;
      else newState = PQ_NORMAL;

      if (newState != pqState) {
        if (pqState != PQ_NORMAL) {
          uint32_t durationMs = millis() - pqEventStartMs;
          Serial.print(F(">>> ")); Serial.print(pqState == PQ_SAG ? F("SAG") : F("SWELL"));
          Serial.print(F(" ended, duration ")); Serial.print(durationMs); Serial.println(F(" ms"));

          String endText = String(pqState == PQ_SAG ? "SAG" : "SWELL") + " ended after " + String(durationMs) + "ms";
          sendWarning(0, endText);
        }
        if (newState != PQ_NORMAL) {
          pqEventStartMs = millis();
          Serial.print(F(">>> ")); Serial.print(newState == PQ_SAG ? F("SAG") : F("SWELL"));
          Serial.print(F(" detected: ")); Serial.print(pctOfNominal, 1); Serial.println(F("% of nominal"));

          uint8_t eventCode = (newState == PQ_SAG) ? 1 : 2;
          String eventText = String(newState == PQ_SAG ? "SAG " : "SWELL ") + String(pctOfNominal, 1) + "% of nominal";
          sendWarning(eventCode, eventText);
        }
        pqState = newState;
      }

      if (newState == PQ_NORMAL) {
        float cycleMidpoint = (cycleMin + cycleMax) / 2.0;
        crossing = crossing * (1.0 - CROSSING_DRIFT_ALPHA) + cycleMidpoint * CROSSING_DRIFT_ALPHA;
      }
    }

    lastInterpCrossingUs = interpCrossingUs;
    haveLastCrossing = true;
    cycleMin = ADC_VREF;
    cycleMax = 0.0;
    cycleSampleCount = 0;
  }

  wasBelowCrossing = isBelowCrossing;
  prevVoltage = voltage;
  prevSampleUs = now;
}
