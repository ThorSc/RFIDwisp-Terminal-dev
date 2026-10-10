#include "moonraker_client.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
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

QidiBoxResult moonrakerReadBox(const String &address, int box) {
  QidiBoxResult result;
  if (box < 1 || box > 4) {
    result.error = "The QIDI box number must be between 1 and 4.";
    return result;
  }

  String url = address + "/printer/objects/query?multi_color_controller&rfid_bridge";
  HTTPClient http;
  http.setTimeout(kTimeoutMs);
  http.setConnectTimeout(kTimeoutMs);
  http.useHTTP10(true); // no chunked transfer: the stream is the plain JSON
  if (!http.begin(url)) {
    result.error = T(StrId::ErrOpenConnection);
    return result;
  }
  int status = http.GET();
  if (status <= 0) {
    result.error = String(T(StrId::ErrNoResponse)) + " " + http.errorToString(status);
    http.end();
    return result;
  }
  if (status >= 400) {
    result.error = "Moonraker returned HTTP " + String(status);
    http.end();
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
  filter["result"]["status"]["rfid_bridge"]["last_raw"] = true;

  JsonDocument doc;
  DeserializationError parseError =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (parseError) {
    result.error = T(StrId::ErrMalformed);
    return result;
  }

  JsonVariantConst printerStatus = doc["result"]["status"];
  JsonVariantConst multiColor = printerStatus["multi_color_controller"];
  if (!multiColor.is<JsonObjectConst>()) {
    result.error = "Moonraker did not return valid QIDI box data.";
    result.unsupported = true;
    return result;
  }

  JsonVariantConst hardware = multiColor["hardware"];
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
  JsonVariantConst lastRaw = printerStatus["rfid_bridge"]["last_raw"];
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

      uint8_t raw[16];
      if (decodeBlock(lastRaw[key], raw)) {
        uint16_t spoolNumber = (uint16_t(raw[14]) << 8) | raw[15];
        slot.spoolNumber = spoolNumber == 0 ? -1 : spoolNumber;
        slot.internalVendorId = raw[13] == 0 ? -1 : raw[13];
      }
    }
    result.slots.push_back(slot);
  }

  result.ok = true;
  return result;
}
