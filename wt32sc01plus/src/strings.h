#pragma once
#include <stddef.h>

// UI text lookup. Every user-facing string in the firmware goes through an
// StrId + T(id), never a literal, so a language can be added without touching
// UI code:
//   - `defaultStrings[]` (strings.cpp) is the built-in English text and
//     `germanStrings[]` the German one; the language is chosen in the
//     settings (AppSettings::language) and applied at startup.
//   - If /lang.json exists on LittleFS ({"read_tag": "Tag lesen", ...}, keyed
//     by the `key` column of strings.cpp), each entry it contains overrides
//     the matching StrId in whichever language is chosen. This lets a string
//     be changed (or a language not built in be tried) without a firmware
//     rebuild.
enum class StrId {
  AppTitle,
  Ready,
  StatusHoldTag,
  ReadTag,
  WriteTag,
  Back,
  Save,
  SpoolData,
  Material,
  Color,
  Vendor,
  SpoolNumber,
  Weight,
  Settings,
  ErrorNoTag,
  ErrorNotClassic1k,
  ErrorAuthFailed,
  ErrorReadFailed,
  ErrorWriteFailed,
  ErrorUnknownTag,
  TagBlank,
  TagRead,
  TagWritten,
  Rc522NotFound,
  WifiStatus,
  SpoolmanAddress,
  UseSpoolman,
  ReconfigureNetwork,
  Spool,
  NewSpool,
  SpoolmanVendor,
  SpoolmanSkipped,
  SpoolCreated,
  SpoolmanCreateFailed,
  LoadingSpoolmanData,
  SpoolmanLoadFailed,
  Filament,
  NoFilament,
  FilterAll,
  NoTemplate,
  ErrorWeightRange,
  ErrorSpoolNumberRange,
  ErrorVendorIdRange,
  SleepTimeout,
  OtaUpdating,
  OtaUpdateFailed,
  UpdateCheckNow,
  UpdateAtStartup,
  UpdateChecking,
  UpdateUpToDate,
  UpdateAvailable,
  UpdateDialogTitle,
  UpdateInstall,
  UpdateLater,
  UpdateInstalling,
  UpdateCheckFailed,
  UpdateInstallFailed,
  Firmware,
  TabGeneral,
  TabWifi,
  TabPrinter,
  TabSpoolman,
  PrinterNone,
  PrinterName,
  PrinterAddress,
  FieldRequired,
  AddressInvalid,
  EditTag,
  BoxLabel,
  ColMaterial,
  ColSlot,
  ColNumber,
  ColSpoolmanVendor,
  ColWeight,
  NoPrinterConfigured,
  PrinterChecking,
  PrinterNotConnected,
  BoxError,
  NoBox,
  PrinterNotSupported,
  PrinterNotSupportedInfo,
  Language,
  Restarting,
  ConnectingWifi,
  WifiNotConnected,
  PortalSpoolman,
  PortalOtaPassword,
  ErrWifiNotConnected,
  ErrNoTime,
  ErrOpenConnection,
  ErrGithubHttp,
  ErrNoResponse,
  ErrMalformed,
  ErrNoFirmwareImage,
  ErrDownloadHttp,
  ErrUnknownSize,
  ErrNoSpace,
  ErrDownloadInterrupted,
  ErrFlashWrite,
  ErrDamaged,
  Count // sentinel, not a real string
};

void stringsInit();                 // mounts LittleFS and loads /lang.json if present
const char *T(StrId id);            // current text for id (override or English default)

// The languages built into the firmware; the first is the default. [name] is
// written in the language itself and not translated.
struct Language {
  const char *code;
  const char *name;
};
extern const Language languages[];
extern const size_t languagesCount;
