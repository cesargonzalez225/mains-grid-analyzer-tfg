
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

uint32_t attempts = 0;
uint32_t successes = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  radio.setRfSwitchPins(RXEN, TXEN);

  Serial.println(F("Repeatedly calling begin() to measure SPI/BUSY/NRST reliability."));
  Serial.println(F("Leave everything untouched and let this run for a couple minutes."));
}

void loop() {
  attempts++;
  int state = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR, LORA_SYNC_WORD, LORA_POWER_DBM, 8, 0, true);
  if (state == RADIOLIB_ERR_NONE) {
    successes++;
  }

  Serial.print(F("attempt "));
  Serial.print(attempts);
  Serial.print(F(": "));
  Serial.print(state == RADIOLIB_ERR_NONE ? F("OK") : F("FAIL"));
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F(" (code "));
    Serial.print(state);
    Serial.print(F(")"));
  }
  Serial.print(F("   running total: "));
  Serial.print(successes);
  Serial.print(F("/"));
  Serial.println(attempts);

  delay(500);
}
