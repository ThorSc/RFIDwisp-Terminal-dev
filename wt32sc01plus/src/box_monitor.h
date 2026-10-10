#pragma once
#include <Arduino.h>

// Reads the QIDI box slots of a printer (Moonraker) and the matching Spoolman
// vendor names and spool weights in a background task, so the touch screen
// stays responsive while a printer that is switched off times out.

// What the overview shows for one slot.
struct BoxSlotView {
  bool loaded = false;
  bool active = false;
  String material;
  String qidiVendor;        // "Gen." for Generic, else as reported
  String colorHex;
  int spoolNumber = -1;     // Spoolman spool number from the tag, -1 = none
  String spoolmanVendor;    // empty if no vendor is linked or Spoolman does not know it
  int weightGrams = -1;     // remaining weight from Spoolman, -1 = unknown
};

struct BoxResult {
  uint32_t generation = 0;  // of the request this answers
  int box = 0;              // the box that was asked for (1-4)
  bool ok = false;
  String error;
  bool unsupported = false; // the printer answered but has no QIDI box data of a supported kind
  int boxCount = 0;         // installed boxes (0 = none / not connected)
  BoxSlotView slots[4];     // only valid if ok and box <= boxCount
};

void boxMonitorBegin();

// Asks for the slots of box [box] of the printer at [printerAddress]. If
// [spoolmanAddress] is not empty, vendor names and weights are looked up there.
// Replaces a request that was not started yet. Returns the request's
// generation, to recognise the answer.
uint32_t boxMonitorRequest(const String &printerAddress, int box, const String &spoolmanAddress);

// The generation of the latest request; older answers are out of date.
uint32_t boxMonitorLatestGeneration();

// Fetches the newest finished answer, if there is one (once).
bool boxMonitorTakeResult(BoxResult &out);

// Whether a request is waiting or running.
bool boxMonitorBusy();
