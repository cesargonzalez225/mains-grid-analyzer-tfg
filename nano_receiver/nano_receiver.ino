
#include <RadioLib.h>

#define NSS   10
#define DIO1  2
#define NRST  9
#define BUSY  8
#define RXEN  7
#define TXEN  6
#define ALERT_PIN 4

#define LORA_FREQ_MHZ   915.0
#define LORA_BW_KHZ     125.0
#define LORA_SF         9
#define LORA_CR         7
#define LORA_SYNC_WORD  0x12

SX1262 radio = new Module(NSS, DIO1, NRST, BUSY);

volatile bool receivedFlag = false;

void setFlag() {
  receivedFlag = true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  pinMode(ALERT_PIN, OUTPUT);
  digitalWrite(ALERT_PIN, LOW);

  radio.setRfSwitchPins(RXEN, TXEN);

  Serial.print(F("[SX1262] Initializing ... "));

  int state = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR, LORA_SYNC_WORD, 10, 8, 0, true);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) delay(10);
  }

  radio.setDio1Action(setFlag);

  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("startReceive failed, code "));
    Serial.println(state);
    while (true) delay(10);
  }

  Serial.println(F("Listening..."));
}

void loop() {
  if (!receivedFlag) return;
  receivedFlag = false;

  String packet;
  int state = radio.readData(packet);

  if (state == RADIOLIB_ERR_NONE) {
    int p1 = packet.indexOf('|');
    int p2 = packet.indexOf('|', p1 + 1);
    int p3 = packet.indexOf('|', p2 + 1);

    if (p1 > 0 && p2 > p1 && p3 > p2) {
      String counter = packet.substring(p1 + 1, p2);
      String code    = packet.substring(p2 + 1, p3);
      String text    = packet.substring(p3 + 1);

      Serial.println(F("--- Warning received ---"));
      Serial.print(F("Counter: ")); Serial.println(counter);
      Serial.print(F("Code:    ")); Serial.println(code);
      Serial.print(F("Text:    ")); Serial.println(text);
      Serial.print(F("RSSI:    ")); Serial.print(radio.getRSSI()); Serial.println(F(" dBm"));
      Serial.print(F("SNR:     ")); Serial.print(radio.getSNR()); Serial.println(F(" dB"));
      Serial.println();

      if (code.toInt() != 0) {
        digitalWrite(ALERT_PIN, HIGH);
        delay(200);
        digitalWrite(ALERT_PIN, LOW);
      }
    } else {
      Serial.print(F("Malformed packet: "));
      Serial.println(packet);
    }
  } else {
    Serial.print(F("readData failed, code "));
    Serial.println(state);
  }

  radio.startReceive();
}
