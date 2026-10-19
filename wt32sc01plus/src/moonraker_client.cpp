#include "moonraker_client.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <string.h>
#include "strings.h"

static const uint32_t kTimeoutMs = 5000;

// Text as sent by the printer; numbers (e.g. an RFID colour ID) are kept as
// their digits, anything else is empty.
static String textOrEmpty(JsonVariantConst value) {
  if (value.is<const char *>()) return String(value.as<const char *>());
  if (value.is<long>()) return String(value.as<long>());
  return String();
}

// Decodes the hex string of a 16-byte tag block (rfid_bridge's last_raw);
// false if it is not at least one block of hex.
static bool decodeBlock(JsonVariantConst value, uint8_t out[16]) {
  const char *hex = value.is<const char *>() ? value.as<const char *>() : nullptr;
  if (!hex) return false;
  uint8_t bytes = 0;
  int high = -1;
  for (const char *p = hex; *p; p++) {
    char c = *p;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
    int digit = (c >= '0' && c <= '9')   ? c - '0'
                : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                : (c >= 'A' && c <= 'F') ? c - 'A' + 10
                                         : -1;
    if (digit < 0) return false;
    if (high < 0) {
      high = digit;
    } else {
      if (bytes < 16) out[bytes] = (uint8_t)(high << 4 | digit);
      if (bytes < 255) bytes++;
      high = -1;
    }
  }
  return high < 0 && bytes >= 16;
}

// "slot7" -> 7; -1 for anything else (e.g. "slot-1" = none).
static int slotNumber(JsonVariantConst value) {
  if (!value.is<const char *>()) return -1;
  const char *text = value.as<const char *>();
  if (strncmp(text, "slot", 4) != 0 || text[4] < '0' || text[4] > '9') return -1;
  return atoi(text + 4);
}

// GETs [url] and parses the answer through [filter] into [doc]; on failure
// [error] says why.
static bool getJson(const String &url, JsonDocument &filter, JsonDocument &doc, String &error) {
  HTTPClient http;
  http.setTimeout(kTimeoutMs);
  http.setConnectTimeout(kTimeoutMs);
  http.useHTTP10(true); // no chunked transfer: the stream is the plain JSON
  if (!http.begin(url)) {
    error = T(StrId::ErrOpenConnection);
    return false;
  }
  int status = http.GET();
  if (status <= 0) {
    error = String(T(StrId::ErrNoResponse)) + " " + http.errorToString(status);
    http.end();
    return false;
  }
  if (status >= 400) {
    error = "Moonraker returned HTTP " + String(status);
    http.end();
    return false;
  }
  DeserializationError parseError =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (parseError) {
    error = T(StrId::ErrMalformed);
    return false;
  }
  return true;
}

// Fills the tag data (spool number, vendor id) of [slot] from rfid_bridge's
// last_raw block of the same slot, if there is one.
static void applyTagBlock(QidiSlot &slot, JsonVariantConst lastRaw, const String &key) {
  uint8_t raw[16];
  if (!decodeBlock(lastRaw[key], raw)) return;
  uint16_t spoolNumber = (uint16_t(raw[14]) << 8) | raw[15];
  slot.spoolNumber = spoolNumber == 0 ? -1 : spoolNumber;
  slot.internalVendorId = raw[13] == 0 ? -1 : raw[13];
}

// Newer firmware (Max4 01.01.07.01, Plus4 2.1.1) has no `hardware` and no
// `slots` in multi_color_controller any more: the box count sits in
// `box_count`/`enable_box`, and the resolved names flat in `box` as
// `boxN_filament_info_M_filament_str`/`_vendor_str`/`_color_rgb_str` (see
// _slotsFromBoxInfo in moonraker_api.dart). Whether a slot holds filament then
// comes from the runout sensor of its `box_stepper slotN` (0 = filament), else
// from `slotN` in save_variables.
static QidiBoxResult readFlatBox(const String &address, int box, JsonVariantConst multiColor,
                                 JsonVariantConst lastRaw, QidiBoxResult result) {
  JsonVariantConst flat = multiColor["box"];
  int boxCount = multiColor["box_count"].is<int>() ? multiColor["box_count"].as<int>()
                 : flat["box_count"].is<int>()     ? flat["box_count"].as<int>()
                                                   : -1;
  if (boxCount < 0 || boxCount > 4) {
    result.error = "Moonraker did not return a valid count of installed QIDI boxes.";
    result.unsupported = true;
    return result;
  }
  bool enabled = !(multiColor["enable_box"].is<int>() && multiColor["enable_box"].as<int>() == 0);
  result.boxCount = enabled ? boxCount : 0;
  if (box > result.boxCount) { // nothing to read, but the count is valid
    result.ok = true;
    return result;
  }

  int boxIndex = box - 1;
  String infoKey = "box" + String(boxIndex) + "_filament_info_";
  if (!flat[infoKey + "0_filament_str"].is<const char *>()) {
    result.error = "Moonraker did not return valid QIDI slot data.";
    result.unsupported = true;
    return result;
  }
  int firstSlot = boxIndex * 4;

  // Which slots hold filament and which one feeds: one more request; if it
  // fails, the slots are shown as empty.
  JsonDocument stateFilter;
  String query = "save_variables";
  JsonObject status = stateFilter["result"]["status"].to<JsonObject>();
  JsonObject variablesFilter = status["save_variables"]["variables"].to<JsonObject>();
  variablesFilter["slot_sync"] = true;
  variablesFilter["last_load_slot"] = true;
  for (int number = firstSlot; number < firstSlot + 4; number++) {
    status["box_stepper slot" + String(number)]["runout_button"] = true;
    variablesFilter["slot" + String(number)] = true;
    query += "&box_stepper%20slot" + String(number);
  }
  JsonDocument stateDoc;
  String ignored;
  bool haveState =
      getJson(address + "/printer/objects/query?" + query, stateFilter, stateDoc, ignored);
  JsonVariantConst stateStatus = stateDoc["result"]["status"];
  JsonVariantConst variables = stateStatus["save_variables"]["variables"];

  // As in the app: slot_sync of save_variables (else the operation's active
  // slot), then last_load_slot; "slot-1" means none.
  JsonVariantConst syncSlot =
      variables["slot_sync"].isNull() ? multiColor["operation"]["active_slot"] : variables["slot_sync"];
  int activeSlot = slotNumber(syncSlot);
  if (activeSlot < 0) activeSlot = slotNumber(variables["last_load_slot"]);

  for (int number = firstSlot; number < firstSlot + 4; number++) {
    String key = "slot" + String(number);
    QidiSlot slot;
    slot.slot = number;

    JsonVariantConst runout = stateStatus["box_stepper " + key]["runout_button"];
    bool loaded = runout.is<int>() ? runout.as<int>() == 0
                                   : (haveState && variables[key].is<int>() && variables[key].as<int>() == 1);
    if (loaded) {
      String prefix = infoKey + String(number - firstSlot) + "_";
      slot.loaded = true;
      slot.active = number == activeSlot;
      slot.material = textOrEmpty(flat[prefix + "filament_str"]);
      slot.vendor = textOrEmpty(flat[prefix + "vendor_str"]);
      JsonVariantConst color = flat[prefix + "color_rgb_str"]; // a list with one "#RRGGBB"
      slot.colorHex = color.is<JsonArrayConst>() ? textOrEmpty(color[0]) : textOrEmpty(color);
      applyTagBlock(slot, lastRaw, key);
    }
    result.slots.push_back(slot);
  }

  result.ok = true;
  return result;
}

QidiBoxResult moonrakerReadBox(const String &address, int box) {
  QidiBoxResult result;
  if (box < 1 || box > 4) {
    result.error = "The QIDI box number must be between 1 and 4.";
    return result;
  }

  // The answer holds the data of every slot and much more; keep only what is
  // shown.
  JsonDocument filter;
  JsonObject controller = filter["result"]["status"]["multi_color_controller"].to<JsonObject>();
  controller["hardware"]["box_count"] = true;
  controller["hardware"]["connected"] = true;
  controller["slots"]["states"] = true;
  JsonObject material = controller["slots"]["materials"]["*"].to<JsonObject>();
  material["filament"]["filament"] = true;
  material["vendor"] = true;
  material["color"] = true;
  // Newer firmware: the flat data of the requested box only.
  controller["box_count"] = true;
  controller["enable_box"] = true;
  controller["operation"]["active_slot"] = true;
  JsonObject flatFilter = controller["box"].to<JsonObject>();
  flatFilter["box_count"] = true;
  for (int m = 0; m < 4; m++) {
    String prefix = "box" + String(box - 1) + "_filament_info_" + String(m) + "_";
    flatFilter[prefix + "filament_str"] = true;
    flatFilter[prefix + "vendor_str"] = true;
    flatFilter[prefix + "color_rgb_str"] = true;
  }
  filter["result"]["status"]["rfid_bridge"]["last_raw"] = true;

  JsonDocument doc;
  if (!getJson(address + "/printer/objects/query?multi_color_controller&rfid_bridge", filter, doc,
               result.error)) {
    return result;
  }

  JsonVariantConst printerStatus = doc["result"]["status"];
  JsonVariantConst multiColor = printerStatus["multi_color_controller"];
  if (!multiColor.is<JsonObjectConst>()) {
    result.error = "Moonraker did not return valid QIDI box data.";
    result.unsupported = true;
    return result;
  }
  JsonVariantConst lastRaw = printerStatus["rfid_bridge"]["last_raw"];

  JsonVariantConst hardware = multiColor["hardware"];
  if (!hardware.is<JsonObjectConst>()) {
    return readFlatBox(address, box, multiColor, lastRaw, result);
  }
  if (!hardware["box_count"].is<int>() || !hardware["connected"].is<bool>()) {
    result.error = "Moonraker did not return a valid QIDI hardware status.";
    result.unsupported = true;
    return result;
  }
  int boxCount = hardware["box_count"].as<int>();
  if (boxCount < 0 || boxCount > 4) {
    result.error = "Moonraker returned an invalid QIDI box count: " + String(boxCount) + ".";
    return result;
  }
  result.boxCount = hardware["connected"].as<bool>() ? boxCount : 0;
  if (box > result.boxCount) { // nothing to read, but the count is valid
    result.ok = true;
    return result;
  }

  JsonVariantConst states = multiColor["slots"]["states"];
  JsonVariantConst materials = multiColor["slots"]["materials"];
  if (!states.is<JsonObjectConst>()) {
    result.error = "Moonraker did not return valid QIDI slot data.";
    return result;
  }

  int firstSlot = (box - 1) * 4;
  for (int number = firstSlot; number < firstSlot + 4; number++) {
    String key = "slot" + String(number);
    QidiSlot slot;
    slot.slot = number;

    if (!states[key].is<int>()) {
      result.error = "Moonraker did not return a valid state for " + key + ".";
      return result;
    }
    int state = states[key].as<int>();
    if (state != 0) {
      // Any non-zero state means the slot holds filament: 1 = loaded, 2 =
      // loaded and feeding the extruder. Both carry the same material data.
      JsonVariantConst data = materials[key];
      if (!data.is<JsonObjectConst>() || !data["filament"].is<JsonObjectConst>()) {
        result.error = "Moonraker did not return data for active " + key + ".";
        return result;
      }
      slot.loaded = true;
      slot.active = state == 2;
      slot.material = textOrEmpty(data["filament"]["filament"]);
      slot.vendor = textOrEmpty(data["vendor"]);
      slot.colorHex = textOrEmpty(data["color"]);
      applyTagBlock(slot, lastRaw, key);
    }
    result.slots.push_back(slot);
  }

  result.ok = true;
  return result;
}
