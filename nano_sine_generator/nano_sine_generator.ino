
#include <string.h>

#define PWM_PIN 3
#define N_SAMPLES 250

#define SURGE_MAX_PCT 125.0

uint8_t sineTable[N_SAMPLES];
uint8_t sineTableNext[N_SAMPLES];
volatile uint16_t sampleIndex = 0;

volatile int16_t ampScale256 = 256;
volatile uint16_t eventCyclesRemaining = 0;

float h3Percent = 0.0;
float h5Percent = 0.0;

void buildSineTable() {

  float peak = 0.0;
  for (int i = 0; i < N_SAMPLES; i++) {
    float theta = (2.0 * PI * i) / N_SAMPLES;
    float v = sin(theta) + h3Percent * sin(3.0 * theta) + h5Percent * sin(5.0 * theta);
    float av = v < 0 ? -v : v;
    if (av > peak) peak = av;
  }
  if (peak < 0.01) peak = 1.0;

  float headroom = SURGE_MAX_PCT / 100.0;
  for (int i = 0; i < N_SAMPLES; i++) {
    float theta = (2.0 * PI * i) / N_SAMPLES;
    float v = sin(theta) + h3Percent * sin(3.0 * theta) + h5Percent * sin(5.0 * theta);
    sineTableNext[i] = (uint8_t)(128 + (127.0 / peak / headroom) * v);
  }

  noInterrupts();
  memcpy(sineTable, sineTableNext, sizeof(sineTable));
  interrupts();
}

void printStatus() {
  Serial.println(F("--- generator status ---"));
  Serial.print(F("3rd harmonic: ")); Serial.print(h3Percent * 100.0, 1); Serial.println(F("%"));
  Serial.print(F("5th harmonic: ")); Serial.print(h5Percent * 100.0, 1); Serial.println(F("%"));
  Serial.print(F("amplitude:    ")); Serial.print(ampScale256 / 2.56, 1); Serial.println(F("% of nominal"));
  Serial.print(F("event cycles remaining: ")); Serial.println(eventCyclesRemaining);
  Serial.println(F("------------------------"));
}

void handleDipSurge(String args, bool isDip) {
  int commaIdx = args.indexOf(',');
  if (commaIdx < 0) {
    Serial.println(F("format: DIP<pct>,<cycles> or SURGE<pct>,<cycles>"));
    return;
  }

  float pct = args.substring(0, commaIdx).toFloat();
  int cycles = args.substring(commaIdx + 1).toInt();
  if (cycles < 1) cycles = 1;

  if (isDip) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
  } else {
    if (pct < 100) pct = 100;
    if (pct > SURGE_MAX_PCT) {
      Serial.print(F("clamped surge to safe max "));
      Serial.print(SURGE_MAX_PCT, 0);
      Serial.println(F("% to protect the Pico's ADC input"));
      pct = SURGE_MAX_PCT;
    }
  }

  ampScale256 = (int16_t)(pct * 2.56);
  eventCyclesRemaining = cycles;

  Serial.print(isDip ? F("DIP") : F("SURGE"));
  Serial.print(F(" to ")); Serial.print(pct, 1);
  Serial.print(F("% for ")); Serial.print(cycles); Serial.println(F(" cycles"));
}

void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (line.equalsIgnoreCase("NORMAL")) {
    ampScale256 = 256;
    eventCyclesRemaining = 0;
    Serial.println(F("event cleared, amplitude back to 100%"));
    return;
  }
  if (line == "?") {
    printStatus();
    return;
  }
  if (line.startsWith("DIP") || line.startsWith("dip")) {
    handleDipSurge(line.substring(3), true);
    return;
  }
  if (line.startsWith("SURGE") || line.startsWith("surge")) {
    handleDipSurge(line.substring(5), false);
    return;
  }
  if (line.startsWith("H3") || line.startsWith("h3")) {
    h3Percent = line.substring(2).toFloat() / 100.0;
    Serial.print(F("3rd harmonic set to ")); Serial.print(h3Percent * 100.0, 1); Serial.println(F("%"));
    buildSineTable();
    return;
  }
  if (line.startsWith("H5") || line.startsWith("h5")) {
    h5Percent = line.substring(2).toFloat() / 100.0;
    Serial.print(F("5th harmonic set to ")); Serial.print(h5Percent * 100.0, 1); Serial.println(F("%"));
    buildSineTable();
    return;
  }

  Serial.println(F("unknown command. Use DIP/SURGE<pct>,<cycles>, H3/H5<pct>, NORMAL, or ?"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  buildSineTable();

  pinMode(PWM_PIN, OUTPUT);
  TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS20);
  OCR2B = 128;

  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS11);
  OCR1A = 159;
  TIMSK1 = _BV(OCIE1A);

  Serial.println(F("Generating 50Hz sine on D3 -> RC filter -> divider -> Pico W ADC"));
  Serial.println(F("DIP/SURGE<pct>,<cycles>  H3/H5<pct>  NORMAL  ?"));
}

ISR(TIMER1_COMPA_vect) {
  int16_t centered = (int16_t)sineTable[sampleIndex] - 128;
  int16_t scaled = 128 + (int16_t)(((int32_t)centered * ampScale256) >> 8);
  if (scaled < 0) scaled = 0;
  if (scaled > 255) scaled = 255;
  OCR2B = (uint8_t)scaled;

  sampleIndex++;
  if (sampleIndex >= N_SAMPLES) {
    sampleIndex = 0;

    if (eventCyclesRemaining > 0) {
      eventCyclesRemaining--;
      if (eventCyclesRemaining == 0) {
        ampScale256 = 256;
      }
    }
  }
}

void loop() {
  handleSerial();
}
