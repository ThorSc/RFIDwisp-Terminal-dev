#pragma once
#include <Arduino.h>
#include "qidi_tag.h"

enum class TagResult {
  Ok,
  NoTag,
  NotMifareClassic1k,
  AuthenticationFailed,
  ReadFailed,
  WriteFailed,
};

class Mfrc522Reader {
public:
  bool begin();

  // Waits up to timeoutMs for a tag; true if one answered.
  bool waitForTag(uint32_t timeoutMs = 3000);

  // Non-blocking: true if a tag that has not been handled yet is on the
  // reader (a tag that was read or written is halted and not found again until
  // it was taken away and put back).
  bool pollForTag();

  // Reads the 16-byte spool payload from qidiTagBlock. The tag must already
  // have answered waitForTag().
  TagResult readSpoolBytes(uint8_t out[qidiTagLength]);

  // Writes and reads back the 16-byte spool payload at qidiTagBlock.
  TagResult writeSpoolBytes(const uint8_t data[qidiTagLength]);

private:
  void release(); // halts the tag and stops the crypto
  bool authenticateSector1();

  bool _tagPresent = false;
};
