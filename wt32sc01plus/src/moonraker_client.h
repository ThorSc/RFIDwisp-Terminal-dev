#pragma once
#include <Arduino.h>
#include <vector>

// Minimal Moonraker client for the QIDI box overview (see moonraker_api.dart):
// asks a printer how many QIDI boxes it has and what its four slots hold.
// Blocking HTTP GET, ~5 s timeout - call it from a background task.
//
// Only printers that report a `multi_color_controller` (QIDI Plus4, Max4) are
// supported; the Q2 fallback of the app (slot data in save_variables plus the
// printer's filament catalog) is not ported.

// One material slot of a QIDI box.
struct QidiSlot {
  int slot = 0;           // number over all boxes: box 1 has 0-3, box 2 has 4-7...
  bool loaded = false;    // the slot holds filament
  bool active = false;    // the slot feeds the extruder right now
  String material;        // e.g. "PLA"; empty for an empty slot
  String vendor;          // QIDI vendor as reported, e.g. "Generic" or "QIDI"
  String colorHex;        // "#RRGGBB" as reported; empty for an empty slot
  int spoolNumber = -1;   // Spoolman spool number from the tag (rfid_bridge), -1 = none
  int internalVendorId = -1; // Spoolman vendor id from the tag, -1 = none
};

struct QidiBoxResult {
  bool ok = false;
  String error;
  // Moonraker answered, but the printer has no QIDI box data of the kind this
  // client reads (e.g. a Q2, or a printer of another make).
  bool unsupported = false;
  int boxCount = 0;               // installed boxes, 0-4 (0 = box hardware not connected)
  std::vector<QidiSlot> slots;    // the four slots of the requested box
};

// Reads the box count and the slots of box [box] (1-4) from the printer at
// [address] (e.g. "http://192.168.1.50:7125").
QidiBoxResult moonrakerReadBox(const String &address, int box);
