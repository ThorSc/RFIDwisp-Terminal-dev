#include "strings.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// clang-format off
static const char *const keys[(int)StrId::Count] = {
  "app_title",
  "ready",
  "status_hold_tag",
  "read_tag",
  "write_tag",
  "back",
  "save",
  "spool_data",
  "material",
  "color",
  "vendor",
  "spool_number",
  "weight",
  "settings",
  "error_no_tag",
  "error_not_classic_1k",
  "error_auth_failed",
  "error_read_failed",
  "error_write_failed",
  "error_unknown_tag",
  "tag_blank",
  "tag_read",
  "tag_written",
  "rc522_not_found",
  "wifi_status",
  "spoolman_address",
  "use_spoolman",
  "reconfigure_network",
  "spool",
  "new_spool",
  "spoolman_vendor",
  "spoolman_skipped",
  "spool_created",
  "spoolman_create_failed",
  "loading_spoolman_data",
  "spoolman_load_failed",
  "filament",
  "no_filament",
  "error_weight_range",
  "error_spool_number_range",
  "error_vendor_id_range",
  "sleep_timeout",
  "ota_updating",
  "ota_update_failed",
  "update_check_now",
  "update_at_startup",
  "update_checking",
  "update_up_to_date",
  "update_available",
  "update_dialog_title",
  "update_install",
  "update_later",
  "update_installing",
  "update_check_failed",
  "update_install_failed",
  "firmware",
  "tab_general",
  "tab_wifi",
  "tab_printer",
  "tab_spoolman",
  "printer_none",
  "printer_name",
  "printer_address",
  "field_required",
  "address_invalid",
  "edit_tag",
  "box",
  "slot",
  "col_material",
  "col_color",
  "col_number",
  "col_spoolman_vendor",
  "col_weight",
  "no_printer_configured",
  "printer_checking",
  "printer_not_connected",
  "box_error",
  "no_box",
  "printer_not_supported",
  "printer_not_supported_info",
};

static const char *const defaultStrings[(int)StrId::Count] = {
  "RFID Wisp Terminal",
  "Ready.",
  "Please place a tag...",
  "Read tag",
  "Write tag",
  "Back",
  "Save",
  "Spool data",
  "Material",
  "Color",
  "Vendor",
  "Spool number",
  "Weight (g)",
  "Settings",
  "No tag present.",
  "Not a MIFARE Classic 1K tag.",
  "Authentication failed (sector 1).",
  "Read failed.",
  "Write failed or not confirmed.",
  "Tag does not hold valid RFIDwisp spool data.",
  "Tag is blank (not written).",
  "Tag read.",
  "Tag written.",
  "RC522 not found - check wiring.",
  "Wi-Fi status",
  "Spoolman address",
  "Use Spoolman",
  "Reconfigure network",
  "Spool",
  "New spool",
  "Spoolman vendor",
  "No matching Spoolman filament - wrote tag without a Spoolman link.",
  "Spool created in Spoolman.",
  "Could not create Spoolman spool - wrote tag without a Spoolman link.",
  "Loading Spoolman data...",
  "Could not load Spoolman data.",
  "Filament",
  "(pick to prefill)",
  "The weight must be between 0 and 10000 g.",
  "The spool number must be between 0 and 999. Spoolman spool:",
  "The Spoolman vendor ID must be between 0 and 255. Vendor ID:",
  "Sleep after (min, 0 = off)",
  "Updating firmware...",
  "OTA update failed.",
  "Check for updates",
  "Check at startup",
  "Checking for updates...",
  "Firmware is up to date.",
  "New version available:",
  "Update available",
  "Install",
  "Later",
  "Installing update - do not switch off.",
  "Update check failed:",
  "Update failed:",
  "Firmware",
  "General",
  "WiFi",
  "Printer",
  "Spoolman",
  "No printers yet",
  "Name",
  "Moonraker address, e.g. http://192.168.1.50:7125",
  "Name is required.",
  "Invalid address (e.g. http://192.168.1.50:7125).",
  "Edit tag",
  "Box",
  "Slot",
  "Material",
  "Color",
  "No.",
  "Spoolman vendor",
  "Weight",
  "No printer configured - add one in Settings.",
  "Checking printer...",
  "not connected",
  "QIDI box data not available:",
  "No QIDI box connected.",
  "not supported",
  "The printer is connected, but not supported: it has no QIDI box data (QIDI Plus4 / Max4 only).",
};
// clang-format on

static String overrides[(int)StrId::Count];
static bool hasOverride[(int)StrId::Count] = {false};

void stringsInit() {
  if (!LittleFS.begin(true)) return; // format-on-fail; no override file = English

  File file = LittleFS.open("/lang.json", "r");
  if (!file) return;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) return;

  for (int i = 0; i < (int)StrId::Count; i++) {
    JsonVariantConst value = doc[keys[i]];
    if (value.is<const char *>()) {
      overrides[i] = value.as<const char *>();
      hasOverride[i] = true;
    }
  }
}

const char *T(StrId id) {
  int i = (int)id;
  if (i < 0 || i >= (int)StrId::Count) return "";
  return hasOverride[i] ? overrides[i].c_str() : defaultStrings[i];
}
