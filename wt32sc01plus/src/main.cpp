#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include "box_monitor.h"
#include "display_setup.h"
#include "mfrc522_reader.h"
#include "ota_setup.h"
#include "qidi_tag.h"
#include "settings.h"
#include "spoolman_client.h"
#include "strings.h"
#include "ui.h"
#include "updater.h"
#include "wifi_setup.h"

static Mfrc522Reader reader;

// Cached Spoolman data, refreshed whenever the write screen (or a tag read)
// is about to show it. filaments is only used internally here (matching a
// new spool to link); vendors/spools are also handed to the UI for its
// dropdowns.
static std::vector<SpoolmanVendor> spoolmanVendors;
static std::vector<SpoolmanFilament> spoolmanFilaments;
static std::vector<SpoolmanSpool> spoolmanSpools;

// The spool number of the last tag read, if any - used as a fallback when
// writing a "new" spool that no longer matches any cached Spoolman filament
// (mirrors rfid_tag_panel.dart's "Spoolman skipped" path for a re-write).
static int lastReadSpoolNumber = -1;

// The QIDI vendor code (byte 2) of the last tag read, if any. Spoolman mode has
// no QIDI vendor dropdown (its row shows the Spoolman vendor), so a re-write
// keeps what the tag had instead of silently changing it.
static int lastReadVendorCode = -1;

static bool spoolmanActive() {
  return settings.useSpoolman && settings.spoolmanAddress.length() > 0;
}

// Reloads the Spoolman caches and pushes them to the UI; returns whether
// Spoolman mode should be active (false also on a load failure, so the form
// falls back to plain manual entry rather than showing stale/empty lists).
static bool refreshSpoolmanData() {
  if (!spoolmanActive()) return false;

  uiSetStatus(T(StrId::LoadingSpoolmanData));
  displayLoop();

  SpoolmanResult result = spoolmanGetVendors(settings.spoolmanAddress, spoolmanVendors);
  if (result.ok) result = spoolmanGetFilaments(settings.spoolmanAddress, spoolmanFilaments);
  if (result.ok) result = spoolmanGetSpools(settings.spoolmanAddress, spoolmanSpools);
  if (!result.ok) {
    uiSetStatus((String(T(StrId::SpoolmanLoadFailed)) + " " + result.error).c_str());
    return false;
  }

  uiSetSpoolmanLists(spoolmanVendors, spoolmanSpools, spoolmanFilaments);
  return true;
}

static const char *tagResultMessage(TagResult result) {
  switch (result) {
    case TagResult::Ok: return "OK";
    case TagResult::NoTag: return T(StrId::ErrorNoTag);
    case TagResult::NotMifareClassic1k: return T(StrId::ErrorNotClassic1k);
    case TagResult::AuthenticationFailed: return T(StrId::ErrorAuthFailed);
    case TagResult::ReadFailed: return T(StrId::ErrorReadFailed);
    case TagResult::WriteFailed: return T(StrId::ErrorWriteFailed);
  }
  return "";
}

// Called by the UI's "Read tag" button. Blocking: the display will not
// update again until this returns, which is fine for a single explicit
// user action but would need a background task for anything longer-running.
//
// [detected]: the tag was already found by the automatic check, so there is
// nothing to wait for.
static void openEmptyEditScreen(const char *message);

static void readTagAndShow(bool detected) {
  if (!detected) {
    uiSetStatus(T(StrId::StatusHoldTag));
    displayLoop();

    if (!reader.waitForTag(3000)) {
      uiSetStatus(tagResultMessage(TagResult::NoTag));
      return;
    }
  }

  uint8_t raw[qidiTagLength];
  TagResult result = reader.readSpoolBytes(raw);
  if (result != TagResult::Ok) {
    uiSetStatus(tagResultMessage(result));
    return;
  }

  // A blank tag or one without valid spool data cannot be edited, but it can
  // be written: open the empty form and say why it is empty.
  FilamentSpool spool;
  if (FilamentSpool::isBlankTagBytes(raw)) {
    openEmptyEditScreen(T(StrId::TagBlank));
    return;
  }
  if (!FilamentSpool::fromTagBytes(raw, spool)) {
    openEmptyEditScreen(T(StrId::ErrorUnknownTag));
    return;
  }

  lastReadSpoolNumber = spool.spoolNumber > 0 ? spool.spoolNumber : -1;
  lastReadVendorCode = spool.vendorCode;
  bool active = refreshSpoolmanData();
  uiSetSpoolmanMode(active);

  if (active || !spoolmanActive()) uiSetStatus(T(StrId::TagRead));
  uiShowSpool(spool);
}

void handleReadTagRequested() { readTagAndShow(false); }

// Shows the edit screen with empty fields for writing a new tag - refreshes
// the Spoolman lists (if active) so the Spool selector is current, then hands
// off to the UI.
static void openEmptyEditScreen(const char *message) {
  lastReadSpoolNumber = -1;
  lastReadVendorCode = -1;
  bool active = refreshSpoolmanData();
  uiSetSpoolmanMode(active);
  if (active || !spoolmanActive()) uiSetStatus(message);
  uiShowEditScreenForWrite();
}

// Resolves the Spoolman side of a write: for an existing spool, just reuses
// its id/vendor; for a new one, tries to link it to a matching filament and
// create it in Spoolman, falling back to a plain (unlinked) tag write if
// that is not possible. No-op if Spoolman is not active.
static void resolveSpoolmanFields(FilamentSpool &spool) {
  if (!spoolmanActive()) return;

  if (!uiIsNewSpoolSelected()) {
    int id = uiSelectedExistingSpoolId();
    int vendorId = uiSelectedSpoolmanVendorId();
    spool.spoolNumber = id > 0 ? (uint16_t)id : 0;
    spool.internalVendorId = vendorId > 0 ? (uint8_t)vendorId : 0;
    return;
  }

  int vendorId = uiSelectedSpoolmanVendorId();
  // A filament picked by the user is used as it is; otherwise look one up
  // from vendor + material + colour.
  int filamentId = uiSelectedFilamentId();
  if (filamentId < 0 && vendorId >= 0) {
    filamentId = findFilamentId(spoolmanFilaments, vendorId, qidiMaterialName(spool.materialCode),
                                qidiColorHex(spool.colorCode));
  }

  if (filamentId < 0) {
    uiSetStatus(T(StrId::SpoolmanSkipped));
    spool.spoolNumber = lastReadSpoolNumber > 0 ? (uint16_t)lastReadSpoolNumber : 0;
    spool.internalVendorId = 0;
    return;
  }

  int newId;
  SpoolmanResult result =
      spoolmanCreateSpool(settings.spoolmanAddress, filamentId, spool.lastWeightGrams, newId);
  if (!result.ok) {
    uiSetStatus((String(T(StrId::SpoolmanCreateFailed)) + ": " + result.error).c_str());
    spool.spoolNumber = lastReadSpoolNumber > 0 ? (uint16_t)lastReadSpoolNumber : 0;
    spool.internalVendorId = 0;
    return;
  }

  spool.spoolNumber = (uint16_t)newId;
  spool.internalVendorId = (uint8_t)vendorId;
  uiSetStatus(T(StrId::SpoolCreated));
}

// The main screen's "Edit tag" button: the edit screen with empty fields, for
// writing a tag (a tag on the reader never gets here, it is read and shown).
void handleEditTagRequested() { openEmptyEditScreen(T(StrId::Ready)); }

// Called by the edit screen's "Write tag" button.
// Limits of what fits on the tag, as enforced by the PC app
// (filament_spool.dart / rfid_tag_panel.dart). Checked before anything is
// created in Spoolman, so a rejected write leaves no orphan spool behind.
static bool validateBeforeSpoolman(const FilamentSpool &spool) {
  if (spool.lastWeightGrams > 10000) {
    uiSetStatus(T(StrId::ErrorWeightRange));
    return false;
  }
  if (!spoolmanActive()) return true;

  int vendorId = uiSelectedSpoolmanVendorId();
  if (vendorId > 255) {
    uiSetStatus((String(T(StrId::ErrorVendorIdRange)) + " " + vendorId).c_str());
    return false;
  }
  int spoolId = uiSelectedExistingSpoolId();
  if (spoolId > 999) {
    uiSetStatus((String(T(StrId::ErrorSpoolNumberRange)) + " #" + spoolId).c_str());
    return false;
  }
  return true;
}

void handleWriteTagRequested() {
  FilamentSpool spool = uiCurrentSpool();
  if (!validateBeforeSpoolman(spool)) return;
  resolveSpoolmanFields(spool);
  // Byte 2 only ever holds the QIDI vendor (0 = GENERIC, 1 = QIDI); the
  // Spoolman vendor lives in byte 13. Spoolman mode has no QIDI vendor
  // dropdown: a re-write keeps the vendor of the tag that was read, a fresh
  // tag gets GENERIC if linked to a Spoolman vendor (a third-party spool) and
  // QIDI otherwise.
  if (spoolmanActive()) {
    spool.vendorCode = lastReadVendorCode >= 0
                           ? (uint8_t)lastReadVendorCode
                           : qidiVendorCode(spool.internalVendorId != 0 ? "GENERIC" : "QIDI");
  }
  if (spool.spoolNumber > 999) {
    // A newly created spool got a number that does not fit; it exists in
    // Spoolman already, but the tag cannot hold it.
    uiSetStatus((String(T(StrId::ErrorSpoolNumberRange)) + " #" + spool.spoolNumber).c_str());
    return;
  }

  uiSetStatus(T(StrId::StatusHoldTag));
  displayLoop();

  if (!reader.waitForTag(3000)) {
    uiSetStatus(tagResultMessage(TagResult::NoTag));
    return;
  }

  uint8_t raw[qidiTagLength];
  spool.toTagBytes(raw);
  TagResult result = reader.writeSpoolBytes(raw);
  uiSetStatus(result == TagResult::Ok ? T(StrId::TagWritten) : tagResultMessage(result));
}

// The newer firmware found by the last check, if any.
static bool updateAvailable = false;
static UpdateInfo pendingUpdate;

static String firmwareLine() { return String(T(StrId::Firmware)) + " v" + firmwareVersion(); }

// Shows the outcome of a check on the settings screen: the button installs
// the found version, or checks again.
static void showUpdateState(const String &status) {
  String button = updateAvailable ? String(T(StrId::UpdateInstall)) + " v" + pendingUpdate.version
                                  : String(T(StrId::UpdateCheckNow));
  uiSetUpdateState(status.c_str(), button.c_str());
}

// Asks GitHub for a newer firmware and updates the settings screen. With
// [prompt] a found update is also offered in a dialog (the startup check);
// the settings button only reports it, the user is at the button already.
static void checkForUpdate(bool prompt) {
  uiShowBusy(T(StrId::UpdateChecking));
  displayLoop();

  String error;
  UpdateCheck result = updateCheck(pendingUpdate, error);
  uiHideBusy();

  updateAvailable = result == UpdateCheck::Available;
  switch (result) {
    case UpdateCheck::Available:
      showUpdateState(String(T(StrId::UpdateAvailable)) + " v" + pendingUpdate.version);
      if (prompt) uiShowUpdatePrompt(pendingUpdate.version.c_str());
      break;
    case UpdateCheck::UpToDate:
      showUpdateState(firmwareLine() + " - " + T(StrId::UpdateUpToDate));
      break;
    case UpdateCheck::Failed:
      showUpdateState(String(T(StrId::UpdateCheckFailed)) + " " + error);
      break;
  }
}

static void onUpdateProgress(int percent) {
  uiShowBusy(T(StrId::UpdateInstalling), percent);
  displayLoop();
}

// Downloads and flashes the update found by the last check; restarts the
// device on success.
static void installUpdate() {
  if (!updateAvailable) return;
  uiShowBusy(T(StrId::UpdateInstalling), 0);
  displayLoop();

  String error;
  bool ok = updateInstall(pendingUpdate, onUpdateProgress, error);
  uiHideBusy();
  if (!ok) showUpdateState(String(T(StrId::UpdateInstallFailed)) + " " + error);
}

// The UI callbacks only record what was asked for. Checking and installing
// block for a long time and redraw the screen themselves (displayLoop), which
// LVGL ignores while it is still inside the event handler that started them:
// the "installing" overlay would never show up. loop() runs them instead.
enum class UpdateAction { None, Check, Install };
static UpdateAction pendingAction = UpdateAction::None;

// The settings screen's update button: install what was found, else check.
void handleUpdateButtonPressed() {
  pendingAction = updateAvailable ? UpdateAction::Install : UpdateAction::Check;
}

void handleInstallUpdateRequested() { pendingAction = UpdateAction::Install; }

static void runPendingUpdateAction() {
  UpdateAction action = pendingAction;
  pendingAction = UpdateAction::None;
  if (action == UpdateAction::Check) checkForUpdate(false);
  if (action == UpdateAction::Install) installUpdate();
}

// The language chosen in the settings; applied by restarting (the texts are
// built once). Done in loop(), not in the touch handler: the "restarting"
// overlay could not be drawn there.
static String pendingLanguage;

void handleLanguageSelected(const char *code) { pendingLanguage = code; }

static void applyPendingLanguage() {
  if (pendingLanguage.isEmpty()) return;
  settings.language = pendingLanguage;
  settings.save();
  uiShowBusy(T(StrId::Restarting));
  displayLoop();
  delay(400);
  ESP.restart();
}

static bool wifiConnected = false;
static bool readerOk = false; // the RC522 answered at startup

static void connectWifi() {
  uiSetStatus(T(StrId::ConnectingWifi));
  displayLoop();

  bool connected = wifiSetupConnect();
  if (connected) {
    String info = String(T(StrId::WifiStatus)) + ": " + WiFi.SSID() + " (" +
                  WiFi.localIP().toString() + ")";
    uiSetWifiStatus(info.c_str());
    otaSetup();
    wifiConnected = true;
  } else {
    uiSetWifiStatus(T(StrId::WifiNotConnected));
  }
}

// How often the main screen looks for a tag on the reader.
static const uint32_t kTagPollIntervalMs = 250;

// Reads a tag that was put on the reader while the main screen is idle and
// opens the edit screen with its data, without any button press.
static void pollForTag() {
  static uint32_t lastPollMs = 0;
  if (millis() - lastPollMs < kTagPollIntervalMs) return;
  lastPollMs = millis();

  if (!readerOk || pendingAction != UpdateAction::None || !uiIsMainScreenIdle()) return;
  if (!reader.pollForTag()) return;

  displayWake();
  readTagAndShow(true);
}

// ---------------------------------------------------------------------------
// QIDI box overview on the main screen. The printer is asked in a background
// task (box_monitor.cpp); loop() picks up the answer.
// ---------------------------------------------------------------------------

static int boxNumber = 1;      // the box shown, 1-4
static String boxSignature;    // printer + Spoolman the last request was made for

static int selectedPrinterIndex() {
  for (size_t i = 0; i < settings.printers.size(); i++) {
    if (settings.printers[i].id == settings.selectedPrinterId) return (int)i;
  }
  return settings.printers.empty() ? -1 : 0;
}

static String printerName(const Printer &printer) {
  return printer.name.length() ? printer.name : printer.id;
}

static String boxRequestSignature() {
  int index = selectedPrinterIndex();
  if (index < 0) return "";
  const Printer &printer = settings.printers[index];
  return printer.id + "|" + printer.address + "|" + (spoolmanActive() ? settings.spoolmanAddress : "");
}

// Shows the printer drop-down (several printers) or the printer's name.
static void showBoxPrinters() {
  std::vector<String> names;
  for (const Printer &printer : settings.printers) names.push_back(printerName(printer));
  int index = selectedPrinterIndex();
  String info = index < 0 ? String(T(StrId::NoPrinterConfigured))
                          : printerName(settings.printers[index]) + " (" + settings.printers[index].address + ")";
  uiBoxShowPrinters(names, index, info.c_str());
}

static void requestBoxData() {
  boxSignature = boxRequestSignature();
  int index = selectedPrinterIndex();
  if (index < 0) {
    uiBoxShowBoxes(0, 0);
    uiBoxShowState("", false);
    uiBoxShowSlots(0, nullptr);
    return;
  }
  boxMonitorRequest(settings.printers[index].address, boxNumber,
                    spoolmanActive() ? settings.spoolmanAddress : String());
  uiBoxShowState(T(StrId::PrinterChecking), false);
}

// Called from loop(): shows the answer of the background task.
static void pollBoxResult() {
  BoxResult result;
  if (!boxMonitorTakeResult(result)) return;
  if (result.generation != boxMonitorLatestGeneration()) return; // a newer request is under way

  if (!result.ok) {
    uiBoxShowBoxes(0, 0);
    uiBoxShowSlots(0, nullptr);
    // A printer that answers but has no box data is no connection problem:
    // say so, or the user starts looking for one.
    String detail = result.unsupported ? String(T(StrId::PrinterNotSupportedInfo))
                                       : String(T(StrId::BoxError)) + " " + result.error;
    uiBoxShowError(T(result.unsupported ? StrId::PrinterNotSupported : StrId::PrinterNotConnected), detail.c_str());
    return;
  }
  if (result.boxCount == 0) {
    uiBoxShowBoxes(0, 0);
    uiBoxShowSlots(0, nullptr);
    uiBoxShowState(T(StrId::NoBox), false);
    return;
  }
  if (boxNumber > result.boxCount) { // the box is gone: back to the first one
    boxNumber = 1;
    requestBoxData();
    return;
  }
  uiBoxShowBoxes(result.boxCount, boxNumber);
  uiBoxShowState("", false);
  uiBoxShowSlots((boxNumber - 1) * 4, result.slots);
}

void handleBoxPrinterSelected(int index) {
  if (index < 0 || index >= (int)settings.printers.size()) return;
  settings.selectedPrinterId = settings.printers[index].id;
  settings.save();
  boxNumber = 1;
  uiBoxShowSlots(0, nullptr);
  requestBoxData();
}

void handleBoxSelected(int box) {
  boxNumber = box;
  requestBoxData();
}

void handleBoxRefreshRequested() { requestBoxData(); }

// The settings screen was left: printers or Spoolman may have changed.
void handleSettingsClosed() {
  showBoxPrinters();
  if (boxRequestSignature() != boxSignature) {
    boxNumber = 1;
    uiBoxShowSlots(0, nullptr);
    requestBoxData();
  }
}

void setup() {
  Serial.begin(115200);

  settings.load();
  stringsInit(); // after the settings: they hold the language

  displaySetup();
  displaySetSleepTimeout(settings.sleepMinutes);
  uiInit();

  boxMonitorBegin();
  connectWifi();

  readerOk = reader.begin();
  if (!readerOk) {
    uiSetStatus(T(StrId::Rc522NotFound));
  } else {
    uiSetStatus(T(StrId::StatusHoldTag));
  }

  showBoxPrinters();
  requestBoxData();

  if (wifiConnected && settings.checkForUpdates) checkForUpdate(true);
}

void loop() {
  displayLoop();
  runPendingUpdateAction();
  applyPendingLanguage();
  pollForTag();
  pollBoxResult();
  otaLoop();
  delay(5);
}
