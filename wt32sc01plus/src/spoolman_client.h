#pragma once
#include <Arduino.h>
#include <map>
#include <vector>

// Minimal Spoolman REST client (see spoolman_api.dart): enough to list
// vendors/filaments/spools, look up one spool's remaining weight, find a
// filament to link a new spool to, and create that spool. HTTP GET/POST
// only, blocking, ~5s timeout.

struct SpoolmanVendor {
  int id = 0;
  String name;
};

struct SpoolmanFilament {
  int id = 0;
  int vendorId = 0;
  String vendorName;
  String name;      // e.g. "Galaxy Black"; may be empty
  String material;  // free-form, e.g. "PLA", "PETG Basic"
  String colorHex;   // "#RRGGBB", may be empty
};

struct SpoolmanSpool {
  int id = 0;
  int filamentId = 0;
  int vendorId = 0;
  String vendorName;
  String material;
  String colorHex;
  int remainingWeightGrams = 0;
};

struct SpoolmanResult {
  bool ok = false;
  String error;
};

SpoolmanResult spoolmanGetVendors(const String &address, std::vector<SpoolmanVendor> &out);
SpoolmanResult spoolmanGetFilaments(const String &address, std::vector<SpoolmanFilament> &out);
SpoolmanResult spoolmanGetSpools(const String &address, std::vector<SpoolmanSpool> &out);

// Remaining weight in whole grams of every spool, by spool id (a lighter read
// than spoolmanGetSpools(): only the weight fields are kept).
SpoolmanResult spoolmanGetWeights(const String &address, std::map<int, int> &out);

// Remaining weight of one spool, in whole grams; -1 if not found/unavailable.
int spoolmanGetRemainingWeight(const String &address, int spoolId);

// The first cached filament matching vendorId + material (case-insensitive)
// + colorHex (normalized, '#' optional); -1 if none matches.
int findFilamentId(const std::vector<SpoolmanFilament> &filaments, int vendorId,
                   const String &material, const String &colorHex);

// POST /api/v1/spool. Returns the new spool's id, or -1 on failure (result.ok
// is set accordingly with an error message).
SpoolmanResult spoolmanCreateSpool(const String &address, int filamentId,
                                   int initialWeightGrams, int &newSpoolId);
