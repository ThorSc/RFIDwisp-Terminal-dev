# Changelog

All notable changes to the RFID Wisp Terminal (WT32-SC01 Plus firmware) are
documented in this file. The version is the one in `VERSION`.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Changed

- The status line is at the bottom edge of the main and edit screens.
- The edit screen has a "Read tag" button next to "Write tag" and "Back", to read another tag without
  returning to the main screen.
- The settings screen has tabs like the RFIDwisp app: General (screen sleep, firmware update and version),
  WiFi (status, reconfigure network), Printer and Spoolman (address, use Spoolman).

### Added

- Printers: the Printer tab lists the configured printers (name and Moonraker address) and adds, edits and
  deletes them, like the app. They are stored on the device.

## [0.6.2] - 2026-10-06

### Fixed

- Firmware update: after "Install" (or "Check now") the screen now shows the
  "installing" overlay with the progress bar right away. The download used to
  run inside the touch event handler, where LVGL does not redraw, so the display
  looked frozen until the device restarted.

## [0.6.1] - 2026-10-05

### Changed

- Tag byte 2 is synchronised with the RFIDwisp app: it holds the QIDI vendor code.
  Reading keeps the QIDI vendor of a tag that also has a Spoolman vendor (only an
  unknown byte 2 with byte 13 set, i.e. older tags, reads as GENERIC), the edit
  screen shows it, and a re-write in Spoolman mode keeps it.
- The material list is the 43 entries of the current Plus4 / Max4 / Q2 firmware,
  identical to RFIDwisp.

### Added

- Host tests (`pio test -e native`) that run the tag vectors shared with RFIDwisp.
- Bill of materials for the terminal in `CAD/BOM.md`.

## [0.6.0] - 2026-09-30

### Added

- Firmware self-update from GitHub releases: the latest release is checked at
  startup (switchable) and from Settings, the user is asked before installing,
  the firmware is downloaded over validated TLS, its SHA-256 is verified and it
  is flashed to the other OTA slot. Releases now also carry the bare
  `firmware.bin` the device downloads.

### Fixed

- Settings screen: more compact layout, the update status line no longer overlaps
  the button.

## [0.5.4] - 2026-09-29

### Changed

- App title is now "RFID Wisp Terminal".

## [0.5.3] - 2026-09-29

### Changed

- App title changed to "RFID Wisp ESP32 Terminal" (replaced again in 0.5.4).

## [0.5.2] - 2026-09-29

### Changed

- The Filament dropdown is sorted by vendor, material, colour, like in the app.

## [0.5.1] - 2026-09-29

### Changed

- The Spool dropdown shows the newest Spoolman spool first, like in the app.

## [0.5.0] - 2026-09-29

### Added

- OTA firmware updates over Wi-Fi.
- The hostname is `RFIDwisp-mobile`.

## [0.4.0] - 2026-09-28

### Changed

- The RFID reader is an RC522 over SPI instead of the PN532 over I2C
  (library: Arduino_MFRC522v2). **The wiring changes**, see the README.

## [0.3.0] - 2026-09-27

### Added

- Screen sleep: the backlight turns off after a configurable idle time
  (Settings, 0–60 min, 0 = never); the waking touch is not passed on to the UI.
- Picking a Spoolman filament for a new spool fixes material, colour and Spoolman
  vendor (only the weight stays editable); that filament is used as it is when
  the spool is created.
- The firmware version is shown next to the title.

## [0.2.0] - 2026-09-27

### Added

- Colour pickers: the Spool and Filament lists show each entry's colour;
  selecting a spool also selects its filament, and a filament sets the colour
  (8-digit RRGGBBAA Spoolman colours are recognised).
- Validation before writing: weight 0–10000, Spoolman vendor ID ≤ 255, spool
  number ≤ 999 (checked before a spool is created in Spoolman).

### Changed

- Landscape UI (480×320) with all screens re-laid out; write screen with four
  rows, colour button and colour grid, numeric keypad overlay for number fields,
  Spoolman address editable in Settings.
- Byte 2 holds only the QIDI vendor (0 GENERIC / 1 QIDI); the Spoolman vendor
  stays in byte 13.
- The Moonraker client, the QIDI Data screen and the printer management were removed.

### Fixed

- The display uses the board's 8-bit parallel bus (it was wrongly driven as SPI);
  the reader pins were corrected.
- Spoolman float weights are read correctly (the remaining weight showed as 0)
  and the weight is shown in the spool list.

## [0.1.0] - 2026-09-26

### Added

- First version: standalone ESP32-S3 / LVGL reimplementation of the RFIDwisp app
  for the WT32-SC01 Plus. Tag read and write, spool creation in Spoolman and
  Wi-Fi setup through a captive portal.
