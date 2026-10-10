#include "spoolman_client.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "strings.h"

static const uint32_t kTimeoutMs = 5000;

static SpoolmanResult httpJson(const String &method, const String &url,
                               const String &body, JsonDocument &doc,
                               const JsonDocument *filter = nullptr) {
  SpoolmanResult result;

  HTTPClient http;
  http.setTimeout(kTimeoutMs);
  if (!http.begin(url)) {
    result.error = T(StrId::ErrOpenConnection);
    return result;
  }
  http.setConnectTimeout(kTimeoutMs);
  if (method == "POST") http.addHeader("Content-Type", "application/json");

  int status = method == "POST" ? http.POST(body) : http.GET();
  Serial.printf("[spoolman] %s %s -> %d (wifi=%d ip=%s gw=%s rssi=%d)\n", method.c_str(),
                url.c_str(), status, (int)WiFi.status(), WiFi.localIP().toString().c_str(),
                WiFi.gatewayIP().toString().c_str(), WiFi.RSSI());
  if (status <= 0) {
    result.error = String(T(StrId::ErrNoResponse)) + " " + http.errorToString(status);
    http.end();
    return result;
  }
  if (status >= 400) {
    result.error = "Spoolman returned HTTP " + String(status);
    http.end();
    return result;
  }

  DeserializationError error =
      filter ? deserializeJson(doc, http.getStream(), DeserializationOption::Filter(*filter))
             : deserializeJson(doc, http.getStream());
  http.end();
  if (error) {
    result.error = "Malformed JSON response.";
    return result;
  }

  result.ok = true;
  return result;
}

static String normalizeColor(const String &hex) {
  String value = hex;
  if (value.startsWith("#")) value = value.substring(1);
  value.toUpperCase();
  return value;
}

// Spoolman sends weights as floats (e.g. 850.5): read them as float, not int.
// remaining_weight if present, else initial_weight - used_weight; 0 if unknown.
static int spoolWeightGrams(JsonVariantConst spool) {
  JsonVariantConst remaining = spool["remaining_weight"];
  if (!remaining.isNull()) return (int)(remaining.as<float>() + 0.5f);
  JsonVariantConst initial = spool["initial_weight"];
  if (!initial.isNull()) {
    float value = initial.as<float>() - (float)(spool["used_weight"] | 0.0f);
    return value > 0 ? (int)(value + 0.5f) : 0;
  }
  return 0;
}

SpoolmanResult spoolmanGetVendors(const String &address, std::vector<SpoolmanVendor> &out) {
  out.clear();
  JsonDocument doc;
  SpoolmanResult result = httpJson("GET", address + "/api/v1/vendor", "", doc);
  if (!result.ok) return result;

  for (JsonVariantConst item : doc.as<JsonArrayConst>()) {
    SpoolmanVendor vendor;
    vendor.id = item["id"] | 0;
    vendor.name = (const char *)(item["name"] | "");
    if (vendor.id > 0 && vendor.name.length() > 0) out.push_back(vendor);
  }
  return result;
}

SpoolmanResult spoolmanGetFilaments(const String &address, std::vector<SpoolmanFilament> &out) {
  out.clear();
  JsonDocument doc;
  SpoolmanResult result = httpJson("GET", address + "/api/v1/filament", "", doc);
  if (!result.ok) return result;

  for (JsonVariantConst item : doc.as<JsonArrayConst>()) {
    SpoolmanFilament filament;
    filament.id = item["id"] | 0;
    filament.material = (const char *)(item["material"] | "");
    filament.colorHex = (const char *)(item["color_hex"] | "");
    filament.vendorId = item["vendor"]["id"] | 0;
    filament.vendorName = (const char *)(item["vendor"]["name"] | "");
    out.push_back(filament);
  }
  return result;
}

SpoolmanResult spoolmanGetSpools(const String &address, std::vector<SpoolmanSpool> &out) {
  out.clear();
  JsonDocument doc;
  SpoolmanResult result = httpJson("GET", address + "/api/v1/spool", "", doc);
  if (!result.ok) return result;

  for (JsonVariantConst item : doc.as<JsonArrayConst>()) {
    SpoolmanSpool spool;
    spool.id = item["id"] | 0;
    JsonVariantConst filament = item["filament"];
    spool.filamentId = filament["id"] | 0;
    spool.material = (const char *)(filament["material"] | "");
    spool.colorHex = (const char *)(filament["color_hex"] | "");
    spool.vendorId = filament["vendor"]["id"] | 0;
    spool.vendorName = (const char *)(filament["vendor"]["name"] | "");
    spool.remainingWeightGrams = spoolWeightGrams(item);
    out.push_back(spool);
  }
  return result;
}

SpoolmanResult spoolmanGetWeights(const String &address, std::map<int, int> &out) {
  out.clear();
  // Only id and the weight fields are kept: the full answer holds every spool
  // with its filament and vendor, which can be a lot.
  JsonDocument filter;
  JsonObject item = filter.add<JsonObject>();
  item["id"] = true;
  item["remaining_weight"] = true;
  item["initial_weight"] = true;
  item["used_weight"] = true;

  JsonDocument doc;
  SpoolmanResult result = httpJson("GET", address + "/api/v1/spool", "", doc, &filter);
  if (!result.ok) return result;

  for (JsonVariantConst spool : doc.as<JsonArrayConst>()) {
    int id = spool["id"] | 0;
    if (id > 0) out[id] = spoolWeightGrams(spool);
  }
  return result;
}

int spoolmanGetRemainingWeight(const String &address, int spoolId) {
  JsonDocument doc;
  SpoolmanResult result =
      httpJson("GET", address + "/api/v1/spool/" + String(spoolId), "", doc);
  if (!result.ok) return -1;
  return spoolWeightGrams(doc.as<JsonVariantConst>());
}

int findFilamentId(const std::vector<SpoolmanFilament> &filaments, int vendorId,
                   const String &material, const String &colorHex) {
  String wantColor = normalizeColor(colorHex);
  for (const SpoolmanFilament &filament : filaments) {
    if (filament.vendorId != vendorId) continue;
    if (!filament.material.equalsIgnoreCase(material)) continue;
    if (wantColor.length() && normalizeColor(filament.colorHex) != wantColor) continue;
    return filament.id;
  }
  return -1;
}

SpoolmanResult spoolmanCreateSpool(const String &address, int filamentId,
                                   int initialWeightGrams, int &newSpoolId) {
  newSpoolId = -1;
  String body = "{\"filament_id\":" + String(filamentId) +
                ",\"initial_weight\":" + String(initialWeightGrams) + "}";
  JsonDocument doc;
  SpoolmanResult result = httpJson("POST", address + "/api/v1/spool", body, doc);
  if (!result.ok) return result;

  int id = doc["id"] | 0;
  if (id <= 0) {
    result.ok = false;
    result.error = "Spoolman did not return a spool id.";
    return result;
  }
  newSpoolId = id;
  return result;
}
