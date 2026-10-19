#include "moonraker_client.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include "moonraker_box.h"
#include "strings.h"

static const uint32_t kTimeoutMs = 5000;

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

QidiBoxResult moonrakerReadBox(const String &address, int box) {
  return moonrakerReadBoxWith(
      [&address](const String &objects, JsonDocument &filter, JsonDocument &doc, String &error) {
        return getJson(address + "/printer/objects/query?" + objects, filter, doc, error);
      },
      box);
}
