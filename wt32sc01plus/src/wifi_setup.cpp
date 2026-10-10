#include "wifi_setup.h"
#include "settings.h"
#include "strings.h"
#include <WiFiManager.h>

// Network hostname (DHCP client id / mDNS name) and OTA device name; also
// used by ota_setup.cpp so the two stay in sync.
const char *const kNetworkHostname = "RFIDwisp-mobile";

static bool portalRanThisBoot = false;

static void onSaveConfig() { portalRanThisBoot = true; }

bool wifiSetupConnect() {
  WiFiManager wm;
  wm.setHostname(kNetworkHostname);

  WiFiManagerParameter customSpoolman(
      "spoolman", T(StrId::PortalSpoolman),
      settings.spoolmanAddress.c_str(), 64);
  WiFiManagerParameter customOtaPassword(
      "otapass", T(StrId::PortalOtaPassword),
      settings.otaPassword.c_str(), 64);

  wm.addParameter(&customSpoolman);
  wm.addParameter(&customOtaPassword);
  wm.setSaveConfigCallback(onSaveConfig);
  wm.setConfigPortalTimeout(300); // give up and retry later rather than hang forever

  bool connected = wm.autoConnect("RFIDwisp-Setup");

  if (portalRanThisBoot) {
    settings.spoolmanAddress = customSpoolman.getValue();
    settings.useSpoolman = settings.spoolmanAddress.length() > 0;
    settings.otaPassword = customOtaPassword.getValue();
    settings.save();
  }

  return connected;
}

void wifiSetupReset() {
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}
