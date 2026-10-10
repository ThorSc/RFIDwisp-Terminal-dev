#include "settings.h"
#include <ArduinoJson.h>
#include <Preferences.h>

AppSettings settings;

static const char *kNamespace = "rfidwisp";

void AppSettings::load() {
  Preferences prefs;
  prefs.begin(kNamespace, true);
  spoolmanAddress = prefs.getString("spoolmanAddr", "");
  useSpoolman = prefs.getBool("useSpoolman", false);
  sleepMinutes = min<uint8_t>(prefs.getUChar("sleepMin", 10), 60);
  otaPassword = prefs.getString("otaPass", "");
  checkForUpdates = prefs.getBool("updCheck", true);
  selectedPrinterId = prefs.getString("printerSel", "");
  language = prefs.getString("lang", "en");
  String printersJson = prefs.getString("printers", "");
  prefs.end();

  printers.clear();
  JsonDocument doc;
  if (printersJson.length() && !deserializeJson(doc, printersJson)) {
    for (JsonVariantConst entry : doc.as<JsonArrayConst>()) {
      Printer printer;
      printer.id = entry["id"] | "";
      printer.name = entry["name"] | "";
      printer.address = entry["address"] | "";
      if (printer.id.length() && printers.size() < kMaxPrinters) printers.push_back(printer);
    }
  }
}

void AppSettings::save() const {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putString("spoolmanAddr", spoolmanAddress);
  prefs.putBool("useSpoolman", useSpoolman);
  prefs.putUChar("sleepMin", sleepMinutes);
  prefs.putString("otaPass", otaPassword);
  prefs.putBool("updCheck", checkForUpdates);

  JsonDocument doc;
  JsonArray list = doc.to<JsonArray>();
  for (const Printer &printer : printers) {
    if (printer.name.isEmpty() || printer.address.isEmpty()) continue; // not filled in yet
    JsonObject entry = list.add<JsonObject>();
    entry["id"] = printer.id;
    entry["name"] = printer.name;
    entry["address"] = printer.address;
  }
  String printersJson;
  serializeJson(doc, printersJson);
  prefs.putString("printers", printersJson);
  prefs.putString("printerSel", selectedPrinterId);
  prefs.putString("lang", language);
  prefs.end();
}
