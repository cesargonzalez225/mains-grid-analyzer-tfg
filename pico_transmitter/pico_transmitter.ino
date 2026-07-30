
#include <RadioLib.h>

#define NSS   17
#define DIO1  20
#define NRST  22
#define BUSY  21
#define RXEN  15
#define TXEN  14

#define LORA_FREQ_MHZ   915.0
#define LORA_BW_KHZ     125.0
#define LORA_SF         9
#define LORA_CR         7
#define LORA_SYNC_WORD  0x12
#define LORA_POWER_DBM  17

#define TX_TIMEOUT_MS 2000

SX1262 radio = new Module(NSS, DIO1, NRST, BUSY);

uint32_t counter = 0;

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

void sendWarning(uint8_t code, const String &text) {
  counter++;
  String packet = "W|" + String(counter) + "|" + String(code) + "|" + text;

  Serial.print(F("Sending: "));
  Serial.println(packet);

  int state = reliableTransmit(packet);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("  transmit success!"));
  } else {
    Serial.print(F("  transmit failed, code "));
    Serial.println(state);
  }
}

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

  Serial.println(F("Ready."));
  Serial.println(F("Type 1, 2 or 3 for a canned warning, or type any other text to send it as code 0."));
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    if (line == "1") {
      sendWarning(1, "FIRE ALARM");
    } else if (line == "2") {
      sendWarning(2, "INTRUSION DETECTED");
    } else if (line == "3") {
      sendWarning(3, "LOW BATTERY");
    } else {
      sendWarning(0, line);
    }
  }
}
