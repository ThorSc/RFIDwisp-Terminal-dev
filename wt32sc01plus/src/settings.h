#pragma once
#include <Arduino.h>
#include <vector>

// A printer (its Moonraker instance), like Printer in the Flutter app.
struct Printer {
  String id;      // stable identifier, e.g. "printer1"
  String name;
  String address; // e.g. http://192.168.1.50:7125
};

constexpr size_t kMaxPrinters = 8;

// The subset of AppSettings from the Flutter app that this firmware needs.
// WiFi credentials are not stored here - WiFiManager keeps those in its own
// NVS namespace.
struct AppSettings {
  String spoolmanAddress = ""; // e.g. http://spoolman.local:7912
  bool useSpoolman = false;
  uint8_t sleepMinutes = 10; // screen sleep timeout, 0-60 minutes, 0 = never
  bool checkForUpdates = true; // look for a new firmware version at startup
  String otaPassword = ""; // OTA update password, empty = no auth required
  String language = "en"; // UI language, see languages[] in strings.cpp
  String selectedPrinterId = ""; // the printer shown on the main screen
  std::vector<Printer> printers; // only complete entries (name and address) are stored

  void load();
  void save() const;
};

extern AppSettings settings;
