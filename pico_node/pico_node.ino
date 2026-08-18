
#define HAS_SENSOR true
#define NODE_ID 1
#define NODE_NAME "House"

#include <RadioLib.h>
#include <stdlib.h>
#include "PQState.h"

#define LORA_NSS   17
#define LORA_DIO1  20
#define LORA_NRST  22
#define LORA_BUSY  21
#define LORA_RXEN  15
#define LORA_TXEN  14
#define ALERT_PIN  13

#define LORA_FREQ_MHZ   915.0
#define LORA_BW_KHZ     125.0
#define LORA_SF         9
#define LORA_CR         7
#define LORA_SYNC_WORD  0x12
#define LORA_POWER_DBM  17

#define LORA_TX_TIMEOUT_MS 2000
#define MIN_LORA_INTERVAL_MS 500
#define HEARTBEAT_INTERVAL_MS 10000

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);
bool radioOk = false;
volatile bool receivedFlag = false;

uint32_t radioCounter = 0;
uint32_t lastLoraTxMs = 0;

int cyclesToSkip = 0;

void setFlag() {
  receivedFlag = true;
}

#define SEEN_CACHE_SIZE 16
struct SeenEntry {
  int32_t nodeId;
  uint32_t counter;
  bool valid;
};
SeenEntry seenCache[SEEN_CACHE_SIZE];
int seenCacheNext = 0;

bool alreadySeen(int32_t nodeId, uint32_t counter) {
  for (int i = 0; i < SEEN_CACHE_SIZE; i++) {
    if (seenCache[i].valid && seenCache[i].nodeId == nodeId && seenCache[i].counter == counter) {
      return true;
    }
  }
  return false;
}

void markSeen(int32_t nodeId, uint32_t counter) {
  seenCache[seenCacheNext].nodeId = nodeId;
  seenCache[seenCacheNext].counter = counter;
  seenCache[seenCacheNext].valid = true;
  seenCacheNext = (seenCacheNext + 1) % SEEN_CACHE_SIZE;
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

uint8_t crc8(const String &data) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < (size_t)data.length(); i++) {
    crc ^= (uint8_t)data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
  }
  return crc;
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
  markSeen(NODE_ID, radioCounter);

  String payload = String(NODE_ID) + "|" + String(radioCounter) + "|" + String(code) + "|" + String(NODE_NAME) + "|" + text;
  String packet = "W|" + payload + "|" + String(crc8(payload));
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
  cyclesToSkip = 2;
}

void relayMessage(int32_t originNodeId, uint32_t counter, uint8_t code, const String &name, const String &text) {
  if (!radioOk) return;

  uint32_t now = millis();
  if (now - lastLoraTxMs < MIN_LORA_INTERVAL_MS) {
    Serial.println(F("LoRa: relay skipped (rate limit)"));
    return;
  }
  lastLoraTxMs = now;

  String payload = String(originNodeId) + "|" + String(counter) + "|" + String(code) + "|" + name + "|" + text;
  String packet = "W|" + payload + "|" + String(crc8(payload));
  Serial.print(F("Relaying: "));
  Serial.println(packet);

  int state = reliableTransmit(packet);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("  relay transmit failed, code "));
    Serial.println(state);
  }
}

#define SENSE_PIN A0

#define ADC_BITS       12
#define ADC_MAX_COUNT  ((1 << ADC_BITS) - 1)
#define ADC_VREF       3.3

#define SAMPLE_INTERVAL_US 200
#define WAVEFORM_DECIMATION 5
#define CAL_DURATION_US    1000000
#define MAX_CAL_SAMPLES    5000
#define CROSSING_DRIFT_ALPHA 0.05
#define CONVERSION_DRIFT_ALPHA 0.02

#define CROSSING_HYSTERESIS_V 0.02

#define FREQ_PLAUSIBLE_MIN_HZ 30.0
#define FREQ_PLAUSIBLE_MAX_HZ 70.0

#define FREQ_MAX_STEP_HZ 3.0

#define FREQ_SLEW_REJECT_LIMIT 3

#define SAG_ENTER_PCT    90.0
#define SAG_EXIT_PCT     93.0
#define SWELL_ENTER_PCT  110.0
#define SWELL_EXIT_PCT   107.0

#define FREQ_ENTER_HZ 0.8
#define FREQ_EXIT_HZ  0.5

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

PQState pqState = PQ_NORMAL;
uint32_t pqEventStartMs = 0;

const char *pqStateName(PQState s) {
  switch (s) {
    case PQ_SAG: return "SAG";
    case PQ_SWELL: return "SWELL";
    case PQ_FREQ_ERROR: return "FREQ_ERROR";
    default: return "NORMAL";
  }
}

PQState classifyPQState(float pctOfNominal, float frequency, PQState prevState) {
  float sagThreshold = (prevState == PQ_SAG) ? SAG_EXIT_PCT : SAG_ENTER_PCT;
  if (pctOfNominal < sagThreshold) return PQ_SAG;

  float swellThreshold = (prevState == PQ_SWELL) ? SWELL_EXIT_PCT : SWELL_ENTER_PCT;
  if (pctOfNominal > swellThreshold) return PQ_SWELL;

  float freqThreshold = (prevState == PQ_FREQ_ERROR) ? FREQ_EXIT_HZ : FREQ_ENTER_HZ;
  if (fabs(frequency - expectedHz) > freqThreshold) return PQ_FREQ_ERROR;

  return PQ_NORMAL;
}

float lastKnownVrms = 0.0;
float lastKnownFreq = 0.0;
bool haveReading = false;
int freqSlewRejectStreak = 0;
uint32_t lastHeartbeatMs = 0;

float readVoltage() {
  int raw = analogRead(SENSE_PIN);
  return (raw * ADC_VREF) / ADC_MAX_COUNT;
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

  crossing = (obsMin + obsMax) / 2.0;

  double sumSq = 0.0;
  int rmsCount = 0;
  for (int i = trim; i <= count - 1 - trim; i++) {
    double ac = calSamples[i] - crossing;
    sumSq += ac * ac;
    rmsCount++;
  }
  float observedRms = (rmsCount > 0) ? (float)sqrt(sumSq / rmsCount) : 0.0;

  if (observedRms < 0.02) {
    Serial.println(F("Calibration FAILED: observed signal under 20mV RMS - check wiring/signal. Keeping previous calibration."));
    return;
  }

  conversionRate = expectedRms / observedRms;

  resetCycleTracking();
  pqState = PQ_NORMAL;

  Serial.print(F("Calibrated: observed ADC range ")); Serial.print(obsMin, 3);
  Serial.print(F("V - ")); Serial.print(obsMax, 3); Serial.println(F("V"));
  Serial.print(F("  crossing = ")); Serial.print(crossing, 3); Serial.println(F(" V"));
  Serial.print(F("  observed RMS = ")); Serial.print(observedRms, 4); Serial.println(F(" V"));
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

  if (line.equalsIgnoreCase("ID")) {
    printIdentity();
    return;
  }

  if (!HAS_SENSOR) {
    Serial.println(F("this node has no sensor - V/H/CAL/W/M/T/? are not applicable here"));
    return;
  }

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
      sendWarning(1, "Test message from node " + String(NODE_ID));
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
      Serial.println(F("unknown command. Use V<rms>, H<hz>, CAL, W, M, T, or ?"));
      break;
  }
}

void doSensingStep() {
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

  bool isBelowCrossing = wasBelowCrossing;
  if (wasBelowCrossing && voltage > crossing + CROSSING_HYSTERESIS_V) {
    isBelowCrossing = false;
  } else if (!wasBelowCrossing && voltage < crossing - CROSSING_HYSTERESIS_V) {
    isBelowCrossing = true;
  }

  if (wasBelowCrossing && !isBelowCrossing) {
    float denom = voltage - prevVoltage;
    float frac = (denom != 0.0) ? (crossing - prevVoltage) / denom : 0.0;
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;
    float interpCrossingUs = prevSampleUs + frac * (float)(now - prevSampleUs);

    if (haveLastCrossing) {
      float periodUs = interpCrossingUs - lastInterpCrossingUs;
      float frequency = 1000000.0 / periodUs;

      if (cyclesToSkip > 0) {
        cyclesToSkip--;
        Serial.println(F("(discarded reading: cycle right after a LoRa transmit, buffer may span the sampling gap)"));
      } else if (frequency < FREQ_PLAUSIBLE_MIN_HZ || frequency > FREQ_PLAUSIBLE_MAX_HZ) {

        Serial.print(F("(discarded implausible freq reading: "));
        Serial.print(frequency, 1);
        Serial.println(F(" Hz)"));
      } else if (haveReading && fabs(frequency - lastKnownFreq) > FREQ_MAX_STEP_HZ
                 && freqSlewRejectStreak < FREQ_SLEW_REJECT_LIMIT) {
        freqSlewRejectStreak++;
        Serial.print(F("(discarded freq reading: "));
        Serial.print(frequency, 2);
        Serial.print(F(" Hz jumped too far from last known "));
        Serial.print(lastKnownFreq, 2);
        Serial.println(F(" Hz)"));
      } else {
        freqSlewRejectStreak = 0;

        double sampleSum = 0.0;
        for (int i = 0; i < cycleSampleCount; i++) sampleSum += cycleBuffer[i];
        double dcOffset = (cycleSampleCount > 0) ? (sampleSum / cycleSampleCount) : crossing;

        double sumSq = 0.0;
        for (int i = 0; i < cycleSampleCount; i++) {
          double ac = cycleBuffer[i] - dcOffset;
          sumSq += ac * ac;
        }
        float rawRms = (cycleSampleCount > 0) ? (float)sqrt(sumSq / cycleSampleCount) : 0.0;
        float realRms = rawRms * conversionRate;

        float measuredVpp = cycleMax - cycleMin;
        float realVpp = measuredVpp * conversionRate;
        float realPeak = realVpp / 2.0;

        Serial.print(F("freq:")); Serial.print(frequency, 2);
        Serial.print(F(" Vpeak:")); Serial.print(realPeak, 1);
        Serial.print(F(" Vrms:")); Serial.print(realRms, 1);
        Serial.print(F(" crossing:")); Serial.println(crossing, 4);

        lastKnownVrms = realRms;
        lastKnownFreq = frequency;
        haveReading = true;

        float pctOfNominal = (realRms / expectedRms) * 100.0;
        PQState newState = classifyPQState(pctOfNominal, frequency, pqState);

        if (newState != pqState) {
          if (pqState != PQ_NORMAL) {
            uint32_t durationMs = millis() - pqEventStartMs;
            Serial.print(F(">>> ")); Serial.print(pqStateName(pqState));
            Serial.print(F(" ended, duration ")); Serial.print(durationMs); Serial.println(F(" ms"));

            String endText = String(pqStateName(pqState)) + " ended after " + String(durationMs) + "ms";
            sendWarning(0, endText);
          }
          if (newState != PQ_NORMAL) {
            pqEventStartMs = millis();
            Serial.print(F(">>> ")); Serial.print(pqStateName(newState));
            Serial.print(F(" detected: ")); Serial.print(pctOfNominal, 1); Serial.println(F("% of nominal"));

            uint8_t eventCode = (newState == PQ_SAG) ? 1 : (newState == PQ_SWELL) ? 2 : 4;
            String eventText = String(pqStateName(newState)) + " ";
            if (newState == PQ_FREQ_ERROR) {
              eventText += String(frequency, 2) + "Hz (target " + String(expectedHz, 1) + "Hz) ";
            } else {
              eventText += String(pctOfNominal, 1) + "% of nominal ";
            }
            eventText += "Vrms=" + String(realRms, 1) + " freq=" + String(frequency, 2);
            sendWarning(eventCode, eventText);
          }
          pqState = newState;
        }

        if (newState == PQ_NORMAL) {
          float cycleMidpoint = (cycleMin + cycleMax) / 2.0;
          crossing = crossing * (1.0 - CROSSING_DRIFT_ALPHA) + cycleMidpoint * CROSSING_DRIFT_ALPHA;

          if (rawRms > 0.02) {
            float impliedConversionRate = expectedRms / rawRms;
            conversionRate = conversionRate * (1.0 - CONVERSION_DRIFT_ALPHA) + impliedConversionRate * CONVERSION_DRIFT_ALPHA;
          }
        }
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

void maybeSendHeartbeat() {
  if (!haveReading) return;
  uint32_t now = millis();

  if (lastHeartbeatMs != 0 && now - lastHeartbeatMs < HEARTBEAT_INTERVAL_MS) return;
  lastHeartbeatMs = now;

  String text = "HB Vrms=" + String(lastKnownVrms, 1) + " freq=" + String(lastKnownFreq, 2);
  sendWarning(3, text);
}

void printIdentity() {
  Serial.print(F("Pico node "));
  Serial.print(NODE_ID);
  Serial.print(F(" \""));
  Serial.print(NODE_NAME);
  Serial.print(F("\""));
  Serial.println(HAS_SENSOR ? F(" - sensing + transmit") : F(" - relay only, no sensor"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  printIdentity();

  if (HAS_SENSOR) {
    analogReadResolution(ADC_BITS);
  }
  pinMode(ALERT_PIN, OUTPUT);
  digitalWrite(ALERT_PIN, LOW);

  radio.setRfSwitchPins(LORA_RXEN, LORA_TXEN);
  Serial.print(F("[SX1262] Initializing ... "));

  int radioState = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR, LORA_SYNC_WORD, LORA_POWER_DBM, 8, 0, true);
  if (radioState == RADIOLIB_ERR_NONE) {
    radioOk = true;
    Serial.println(F("success!"));
  } else {
    radioOk = false;
    Serial.print(F("failed, code "));
    Serial.print(radioState);
    Serial.println(F(" - continuing without LoRa (sensing, if enabled, still works standalone)."));
  }

  if (radioOk) {
    radio.setDio1Action(setFlag);
    int state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
      Serial.print(F("startReceive failed, code "));
      Serial.println(state);
    } else {
      Serial.println(F("Listening..."));
    }
  }

  if (HAS_SENSOR) {
    Serial.println(F("Send V<rms>/H<hz> to change the expected target, or CAL to recalibrate anytime."));
    runCalibration();
  }
}

void handleReceivedPacket() {
  String packet;
  int state = radio.readData(packet);

  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("readData failed, code "));
    Serial.println(state);
    radio.startReceive();
    return;
  }

  int p1 = packet.indexOf('|');
  int p2 = packet.indexOf('|', p1 + 1);
  int p3 = packet.indexOf('|', p2 + 1);
  int p4 = packet.indexOf('|', p3 + 1);
  int p5 = packet.indexOf('|', p4 + 1);
  int p6 = packet.indexOf('|', p5 + 1);

  if (!(p1 > 0 && p2 > p1 && p3 > p2 && p4 > p3 && p5 > p4 && p6 > p5)) {
    Serial.print(F("Malformed packet: "));
    Serial.println(packet);
    radio.startReceive();
    return;
  }

  int32_t originNodeId = packet.substring(p1 + 1, p2).toInt();
  uint32_t counter = (uint32_t)packet.substring(p2 + 1, p3).toInt();
  uint8_t code = (uint8_t)packet.substring(p3 + 1, p4).toInt();
  String name = packet.substring(p4 + 1, p5);
  String text = packet.substring(p5 + 1, p6);
  uint8_t receivedCrc = (uint8_t)packet.substring(p6 + 1).toInt();

  String payload = packet.substring(p1 + 1, p6);
  if (crc8(payload) != receivedCrc) {
    Serial.print(F("Checksum mismatch, dropping corrupted packet: "));
    Serial.println(packet);
    radio.startReceive();
    return;
  }

  if (alreadySeen(originNodeId, counter)) {
    Serial.print(F("(duplicate from node "));
    Serial.print(originNodeId);
    Serial.println(F(", already handled - ignoring)"));
    radio.startReceive();
    return;
  }
  markSeen(originNodeId, counter);

  Serial.println(F("--- Warning received ---"));
  Serial.print(F("From node: ")); Serial.println(originNodeId);
  Serial.print(F("Name:      ")); Serial.println(name);
  Serial.print(F("Counter:   ")); Serial.println(counter);
  Serial.print(F("Code:      ")); Serial.println(code);
  Serial.print(F("Text:      ")); Serial.println(text);
  Serial.print(F("RSSI:      ")); Serial.print(radio.getRSSI()); Serial.println(F(" dBm"));
  Serial.print(F("SNR:       ")); Serial.print(radio.getSNR()); Serial.println(F(" dB"));
  Serial.println();

  if (code != 0 && code != 3) {
    digitalWrite(ALERT_PIN, HIGH);
    delay(200);
    digitalWrite(ALERT_PIN, LOW);
  }

  if (!HAS_SENSOR && originNodeId != NODE_ID) {
    relayMessage(originNodeId, counter, code, name, text);
  }

  radio.startReceive();
}

void loop() {
  handleSerial();

  if (HAS_SENSOR) {
    doSensingStep();
    maybeSendHeartbeat();
  }

  if (receivedFlag) {
    receivedFlag = false;
    handleReceivedPacket();
  }
}
