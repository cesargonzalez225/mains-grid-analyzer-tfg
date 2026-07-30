
#include <RadioLib.h>

#define DIO1_PIN 2

#define NSS   10
#define NRST  9
#define BUSY  8
#define RXEN  7
#define TXEN  6

#define LORA_FREQ_MHZ   915.0
#define LORA_BW_KHZ     125.0
#define LORA_SF         9
#define LORA_CR         7
#define LORA_SYNC_WORD  0x12
#define LORA_POWER_DBM  17

SX1262 radio = new Module(NSS, DIO1_PIN, NRST, BUSY);

volatile uint32_t edgeCount = 0;
volatile uint32_t lastEdgeMs = 0;

void onEdge() {
  edgeCount++;
  lastEdgeMs = millis();
}

uint32_t lastPrinted = 0;
uint32_t lastAttempt = 0;
bool txInFlight = false;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  radio.setRfSwitchPins(RXEN, TXEN);

  Serial.print(F("[SX1262] Initializing ... "));
  int state = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR, LORA_SYNC_WORD, LORA_POWER_DBM, 8, 0, true);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) delay(10);
  }

  pinMode(DIO1_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(DIO1_PIN), onEdge, CHANGE);

  Serial.print(F("Watching pin D"));
  Serial.print(DIO1_PIN);
  Serial.println(F(" for any activity. Starting repeated transmit attempts..."));
}

void loop() {
  uint32_t now = millis();

  if (!txInFlight && now - lastAttempt > 3000) {
    lastAttempt = now;
    txInFlight = true;
    Serial.println(F("--- starting transmit ---"));
    int state = radio.startTransmit("PROBE");
    if (state != RADIOLIB_ERR_NONE) {
      Serial.print(F("startTransmit() itself failed, code "));
      Serial.println(state);
    }
  }

  if (txInFlight && now - lastAttempt > 2000) {
    txInFlight = false;
    radio.standby();
  }

  if (now - lastPrinted > 1000) {
    lastPrinted = now;
    Serial.print(F("edges so far: "));
    Serial.print(edgeCount);
    Serial.print(F("   last edge at t="));
    Serial.print(lastEdgeMs);
    Serial.print(F("ms   pin now reads: "));
    Serial.println(digitalRead(DIO1_PIN));
  }
}
