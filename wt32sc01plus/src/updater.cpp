#include "updater.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>
#include <time.h>

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

const char *const kFirmwareAssetName = "RFIDwisp-ESP32-wt32sc01plus-firmware.bin";

static const char *kLatestReleaseUrl =
    "https://api.github.com/repos/ThorSc/RFIDwisp-Terminal/releases/latest";
static const uint32_t kTimeoutMs = 10000;

// The CA bundle that is part of the Arduino-ESP32 core (Mozilla's roots).
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");

const char *firmwareVersion() { return FIRMWARE_VERSION; }

// "1.2.3" -> {1, 2, 3}; false if it is not a dotted list of numbers.
static bool parseVersion(const char *text, int parts[3]) {
  if (*text == 'v' || *text == 'V') text++;
  parts[0] = parts[1] = parts[2] = 0;
  for (int i = 0; i < 3; i++) {
    if (*text < '0' || *text > '9') return false;
    parts[i] = atoi(text);
    while (*text >= '0' && *text <= '9') text++;
    if (*text != '.') break;
    text++;
  }
  return *text == '\0' || *text == '-' || *text == '+';
}

bool isNewerVersion(const char *latest, const char *current) {
  int a[3], b[3];
  if (!parseVersion(latest, a) || !parseVersion(current, b)) return false;
  for (int i = 0; i < 3; i++) {
    if (a[i] != b[i]) return a[i] > b[i];
  }
  return false;
}

// Certificate validation needs the date; the board has no battery-backed
// clock, so ask NTP.
static bool syncClock() {
  if (time(nullptr) > 1700000000) return true;
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  for (int i = 0; i < 40; i++) {
    if (time(nullptr) > 1700000000) return true;
    delay(250);
  }
  return false;
}

static bool beginGithub(HTTPClient &http, WiFiClientSecure &client, const String &url) {
  client.setCACertBundle(rootca_crt_bundle_start);
  http.setTimeout(kTimeoutMs);
  http.setConnectTimeout(kTimeoutMs);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS); // release files redirect to a CDN
  http.setUserAgent("RFIDwisp-Terminal");
  return http.begin(client, url);
}

UpdateCheck updateCheck(UpdateInfo &info, String &error) {
  if (WiFi.status() != WL_CONNECTED) {
    error = "Not connected to Wi-Fi.";
    return UpdateCheck::Failed;
  }
  if (!syncClock()) {
    error = "Could not get the time.";
    return UpdateCheck::Failed;
  }

  WiFiClientSecure client;
  HTTPClient http;
  if (!beginGithub(http, client, kLatestReleaseUrl)) {
    error = "Could not open connection.";
    return UpdateCheck::Failed;
  }
  http.addHeader("Accept", "application/vnd.github+json");

  int status = http.GET();
  if (status != 200) {
    error = status > 0 ? "GitHub returned HTTP " + String(status)
                       : "No response: " + http.errorToString(status);
    http.end();
    return UpdateCheck::Failed;
  }

  // The answer is large (release notes, every asset's metadata); keep only
  // what is needed.
  JsonDocument filter;
  filter["tag_name"] = true;
  JsonObject assetFilter = filter["assets"].add<JsonObject>();
  assetFilter["name"] = true;
  assetFilter["browser_download_url"] = true;
  assetFilter["digest"] = true;
  assetFilter["size"] = true;

  JsonDocument doc;
  DeserializationError parseError =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (parseError) {
    error = "Malformed response.";
    return UpdateCheck::Failed;
  }

  const char *tag = doc["tag_name"] | "";
  if (tag[0] == '\0') {
    error = "Malformed response.";
    return UpdateCheck::Failed;
  }
  const char *latest = (tag[0] == 'v' || tag[0] == 'V') ? tag + 1 : tag;
  if (!isNewerVersion(latest, firmwareVersion())) return UpdateCheck::UpToDate;

  for (JsonObject asset : doc["assets"].as<JsonArray>()) {
    if (String(asset["name"] | "") != kFirmwareAssetName) continue;
    String url = asset["browser_download_url"] | "";
    if (!url.startsWith("https://")) break; // never download over plain http
    info.version = latest;
    info.url = url;
    info.size = asset["size"] | 0;
    String digest = asset["digest"] | "";
    info.sha256 = digest.startsWith("sha256:") ? digest.substring(7) : String();
    info.sha256.toLowerCase();
    return UpdateCheck::Available;
  }
  error = "The release has no firmware image.";
  return UpdateCheck::Failed;
}

static String hexOf(const uint8_t *bytes, size_t length) {
  static const char digits[] = "0123456789abcdef";
  String out;
  for (size_t i = 0; i < length; i++) {
    out += digits[bytes[i] >> 4];
    out += digits[bytes[i] & 15];
  }
  return out;
}

bool updateInstall(const UpdateInfo &info, void (*progress)(int percent), String &error) {
  if (!syncClock()) {
    error = "Could not get the time.";
    return false;
  }

  WiFiClientSecure client;
  HTTPClient http;
  if (!beginGithub(http, client, info.url)) {
    error = "Could not open connection.";
    return false;
  }
  int status = http.GET();
  if (status != 200) {
    error = status > 0 ? "Download failed: HTTP " + String(status)
                       : "Download failed: " + http.errorToString(status);
    http.end();
    return false;
  }

  size_t total = info.size > 0 ? info.size : (http.getSize() > 0 ? http.getSize() : 0);
  if (total == 0) {
    error = "Unknown file size.";
    http.end();
    return false;
  }
  if (!Update.begin(total)) {
    error = String("Not enough space: ") + Update.errorString();
    http.end();
    return false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[1024];
  size_t received = 0;
  int lastPercent = -1;
  uint32_t lastData = millis();
  bool ok = true;

  while (received < total) {
    size_t available = stream->available();
    if (available == 0) {
      if (!http.connected() || millis() - lastData > kTimeoutMs) {
        error = "Download interrupted.";
        ok = false;
        break;
      }
      delay(1);
      continue;
    }
    size_t count = stream->readBytes(buffer, min(min(available, sizeof(buffer)), total - received));
    if (count == 0) continue;
    lastData = millis();
    if (Update.write(buffer, count) != count) {
      error = String("Flash write failed: ") + Update.errorString();
      ok = false;
      break;
    }
    mbedtls_sha256_update(&sha, buffer, count);
    received += count;

    int percent = (int)(received * 100 / total);
    if (percent != lastPercent) {
      lastPercent = percent;
      if (progress) progress(percent);
    }
  }
  http.end();

  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);

  // Check before Update.end(): that call is what makes the new image the one
  // to boot.
  if (ok && info.sha256.length() > 0 && hexOf(digest, sizeof(digest)) != info.sha256) {
    error = "The downloaded file is damaged.";
    ok = false;
  }
  if (!ok) {
    Update.abort();
    return false;
  }
  if (!Update.end()) {
    error = String("Update failed: ") + Update.errorString();
    return false;
  }

  delay(300);
  ESP.restart();
  return true;
}
