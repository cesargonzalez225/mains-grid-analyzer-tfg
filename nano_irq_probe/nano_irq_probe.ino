
#include <RadioLib.h>

#define NSS   10
#define DIO1  2
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

SX1262 radio = new Module(NSS, DIO1, NRST, BUSY);

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
}

void loop() {
  Serial.println(F("--- attempt ---"));

  uint32_t t0 = millis();
  int state = radio.startTransmit("PROBE");
  uint32_t t1 = millis();
  Serial.print(F("startTransmit() returned code "));
  Serial.print(state);
  Serial.print(F(" after "));
  Serial.print(t1 - t0);
  Serial.println(F(" ms (this includes the un-timed wait for BUSY/PA ramp-up)"));

  bool done = false;
  uint32_t deadline = t1 + 5000;
  while (millis() < deadline) {
    if (digitalRead(DIO1) == HIGH) {
      done = true;
      break;
    }
  }
  uint32_t t2 = millis();

  uint16_t irq = radio.getIrqStatus();
  Serial.print(F("pin went HIGH: "));
  Serial.print(done ? F("yes") : F("no (gave up after 5000ms)"));
  Serial.print(F("   elapsed since startTransmit(): "));
  Serial.print(t2 - t1);
  Serial.print(F(" ms   raw IRQ status: 0b"));
  Serial.println(irq, BIN);
  Serial.print(F("  TX_DONE bit set: "));
  Serial.println((irq & 0x0001) ? F("YES") : F("no"));
  Serial.print(F("  TIMEOUT bit set: "));
  Serial.println((irq & 0x0200) ? F("YES") : F("no"));

  radio.finishTransmit();
  delay(2000);
}
