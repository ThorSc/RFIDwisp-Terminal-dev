#pragma once
#include <Arduino.h>

// Firmware updates from the GitHub releases of the distribution repo
// (ThorSc/RFIDwisp-Terminal): looks up the latest release, and if it is newer
// than the running firmware, downloads its firmware image and flashes it
// into the other OTA partition. Unlike ota_setup.h (a push from the PC over
// the local network) this is the device pulling the update itself.
//
// The connection to GitHub is TLS with certificate validation (needs the
// clock, which is set via NTP first), and the download is checked against the
// SHA-256 GitHub publishes for the release file before it is activated.

// The release asset the device installs: the app image only (the zip in the
// release holds the full flash layout for a first, USB-cable install).
extern const char *const kFirmwareAssetName;

struct UpdateInfo {
  String version;   // e.g. "0.6.0", without a leading "v"
  String url;       // download URL of the firmware image
  String sha256;    // lower-case hex, empty if GitHub did not publish one
  size_t size = 0;  // bytes, 0 if unknown
};

enum class UpdateCheck { UpToDate, Available, Failed };

// The version of the running firmware ("dev" for a local build).
const char *firmwareVersion();

// Whether version [latest] is higher than [current] ("1.2.3", a leading "v"
// and a "-suffix" are ignored). False if either cannot be parsed.
bool isNewerVersion(const char *latest, const char *current);

// Asks GitHub for the latest release. Blocking, needs Wi-Fi. On Available,
// [info] describes the update; on Failed, [error] says why.
UpdateCheck updateCheck(UpdateInfo &info, String &error);

// Downloads and flashes [info], then restarts into the new firmware (does not
// return on success). [progress] gets 0..100. On failure the running firmware
// is left untouched and [error] says why.
bool updateInstall(const UpdateInfo &info, void (*progress)(int percent), String &error);
