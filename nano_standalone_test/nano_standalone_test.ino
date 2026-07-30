
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

uint32_t counter = 0;

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
    Serial.println(F("Stopping here - check SPI/BUSY/NRST wiring."));
    while (true) delay(10);
  }
}

#define TX_TIMEOUT_MS 2000

int reliableTransmit(String &packet) {
  int state = radio.startTransmit(packet);
  if (state != RADIOLIB_ERR_NONE) {
    return state;
  }

  uint32_t deadline = millis() + TX_TIMEOUT_MS;
  while (digitalRead(DIO1) == LOW) {
    if (millis() > deadline) {
      radio.finishTransmit();
      return RADIOLIB_ERR_TX_TIMEOUT;
    }
  }

  return radio.finishTransmit();
}

void loop() {
  counter++;
  String packet = "TEST|" + String(counter);

  Serial.print(F("[SX1262] Transmitting packet (\""));
  Serial.print(packet);
  Serial.print(F("\") ... "));

  int state = reliableTransmit(packet);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
    Serial.println(F("timeout! (check DIO1 wiring - D2)"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }

  delay(2000);
}
