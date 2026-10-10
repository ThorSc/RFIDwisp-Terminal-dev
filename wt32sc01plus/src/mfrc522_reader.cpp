#include "mfrc522_reader.h"
#include "board_config.h"
#include <SPI.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>

static MFRC522DriverPinSimple ssPin(RC522_SS);
static MFRC522DriverSPI driver{ssPin, SPI};
static MFRC522 rc522{driver};

// Same default keys, in the same try order, as RFIDwisp's mifare.dart:
// each key is tried as key A, then as key B, until one authenticates.
static const uint8_t defaultKeys[][6] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // factory default
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // common default
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}, // MAD key
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5}, // common key
};
static const size_t defaultKeysCount = sizeof(defaultKeys) / sizeof(defaultKeys[0]);

bool Mfrc522Reader::begin() {
  SPI.begin(RC522_SCK, RC522_MISO, RC522_MOSI, RC522_SS);

  if (RC522_RST != -1) { // hardware-reset the module if RST is wired up
    pinMode(RC522_RST, OUTPUT);
    digitalWrite(RC522_RST, LOW);
    delay(2);
    digitalWrite(RC522_RST, HIGH);
    delay(50);
  }

  return rc522.PCD_Init(); // false if the RC522 didn't answer - check wiring/pins
}

bool Mfrc522Reader::waitForTag(uint32_t timeoutMs) {
  _tagPresent = false;
  uint32_t deadline = millis() + timeoutMs;
  do {
    if (rc522.PICC_IsNewCardPresent() && rc522.PICC_ReadCardSerial()) {
      _tagPresent = true;
      return true;
    }
    // A tag that was read earlier is halted and lies on the reader still:
    // wake it up instead of waiting for it to be taken away and put back.
    byte atqa[2];
    byte atqaSize = sizeof(atqa);
    if (rc522.PICC_WakeupA(atqa, &atqaSize) == MFRC522::StatusCode::STATUS_OK &&
        rc522.PICC_ReadCardSerial()) {
      _tagPresent = true;
      return true;
    }
    delay(50);
  } while (millis() < deadline);
  return false;
}

bool Mfrc522Reader::pollForTag() {
  _tagPresent = rc522.PICC_IsNewCardPresent() && rc522.PICC_ReadCardSerial();
  return _tagPresent;
}

void Mfrc522Reader::release() {
  rc522.PICC_HaltA();
  rc522.PCD_StopCrypto1();
}

bool Mfrc522Reader::authenticateSector1() {
  if (!_tagPresent) return false;
  MFRC522::MIFARE_Key key;
  for (size_t i = 0; i < defaultKeysCount; i++) {
    memcpy(key.keyByte, defaultKeys[i], 6);
    for (uint8_t keyType = 0; keyType <= 1; keyType++) { // 0 = key A, 1 = key B
      MFRC522::PICC_Command cmd = keyType == 0
          ? MFRC522::PICC_Command::PICC_CMD_MF_AUTH_KEY_A
          : MFRC522::PICC_Command::PICC_CMD_MF_AUTH_KEY_B;
      if (rc522.PCD_Authenticate(cmd, qidiTagBlock, &key, &(rc522.uid)) ==
          MFRC522::StatusCode::STATUS_OK) {
        return true;
      }
    }
  }
  return false;
}

TagResult Mfrc522Reader::readSpoolBytes(uint8_t out[qidiTagLength]) {
  if (!_tagPresent) return TagResult::NoTag;
  // Whatever happens next, the tag ends up halted so that pollForTag() does
  // not find it again until it was taken away.
  if (rc522.uid.size != 4) { // UL/DESFire etc.
    release();
    return TagResult::NotMifareClassic1k;
  }
  if (!authenticateSector1()) {
    release();
    return TagResult::AuthenticationFailed;
  }

  TagResult result = TagResult::Ok;
  uint8_t buffer[18]; // MIFARE_Read wants room for the trailing CRC_A
  byte bufferSize = sizeof(buffer);
  if (rc522.MIFARE_Read(qidiTagBlock, buffer, &bufferSize) != MFRC522::StatusCode::STATUS_OK) {
    result = TagResult::ReadFailed;
  } else {
    memcpy(out, buffer, qidiTagLength);
  }

  rc522.PICC_HaltA();
  rc522.PCD_StopCrypto1();
  return result;
}

TagResult Mfrc522Reader::writeSpoolBytes(const uint8_t data[qidiTagLength]) {
  if (!_tagPresent) return TagResult::NoTag;
  if (rc522.uid.size != 4) {
    release();
    return TagResult::NotMifareClassic1k;
  }
  if (!authenticateSector1()) {
    release();
    return TagResult::AuthenticationFailed;
  }

  TagResult result = TagResult::Ok;
  if (rc522.MIFARE_Write(qidiTagBlock, (uint8_t *)data, qidiTagLength) != MFRC522::StatusCode::STATUS_OK) {
    result = TagResult::WriteFailed;
  } else {
    uint8_t readBack[18];
    byte bufferSize = sizeof(readBack);
    if (rc522.MIFARE_Read(qidiTagBlock, readBack, &bufferSize) != MFRC522::StatusCode::STATUS_OK ||
        memcmp(readBack, data, qidiTagLength) != 0) {
      result = TagResult::WriteFailed;
    }
  }

  rc522.PICC_HaltA();
  rc522.PCD_StopCrypto1();
  return result;
}
