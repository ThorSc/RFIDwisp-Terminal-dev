# Changelog

All notable changes to the RFID Wisp Terminal (WT32-SC01 Plus firmware) are
documented in this file. The version is the one in `VERSION`.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.8.0] - 2026-10-18

### Changed

- Tag screen, Spoolman filament picker: the list shows the filament name after the material, and an "A-Z"
  button on the left opens a keyboard of digits and letters; the list then shows only the filaments whose vendor starts
  with that character ("#" for other characters, "All" shows every one). The filaments are sorted without regard to upper and lower case (eSun under E). The list builds much faster, and the filaments are fetched with
  only the fields that are used.
  While the list builds (the first opening takes a while) a spinner is shown.
  A text that is too long for its line now ends in "..." instead of wrapping onto a second line (the pickers and their buttons).
  The hint entry "(pick to prefill)" at the top of the filament list is greyed out and cannot be chosen; once a filament is chosen it reads "(no template)" and drops the choice again.
- Main screen: if the printer cannot be reached (or has no QIDI box data), the red note was cut off after the
  first word ("nicht..."). The note and the full message now take the place of the empty slot rows, in red
  and wrapped; the status line no longer carries it.

## [0.7.0] - 2026-10-11

### Changed

- The main screen has one button, "Edit tag"; "Write tag" and "Read tag" are gone from there. "Edit tag" opens
  the edit screen with empty fields, whether a tag lies on the reader or not (a tag on the reader is read and
  shown on its own). A blank tag, or one without valid spool data, also opens the empty edit screen, with the
  reason in the status line.
- A tag that was read earlier and still lies on the reader is found again by "Write tag" / "Read tag" on the
  edit screen without taking it away first.
- A tag put on the reader is detected automatically while the main screen is showing: the terminal reads it and
  opens the edit screen with its data, no button press needed. The tag has to be taken away before the same tag is read again.
- The status line is at the bottom edge of the main and edit screens.
- The edit screen has a "Read tag" button next to "Write tag" and "Back", to read another tag without
  returning to the main screen.
- The settings screen has tabs like the RFIDwisp app: General (screen sleep, firmware update and version),
  WiFi (status, reconfigure network), Printer and Spoolman (address, use Spoolman).

### Added

- Languages: English and German. A list with flags in Settings > General switches the language (the terminal
  restarts to apply it); the choice is stored on the device. The error messages of the firmware update, the
  Wi-Fi texts and the setup portal's labels are translated as well. The fonts are now rendered from a Montserrat
  TrueType file in flash, so umlauts work.
- QIDI box overview on the main screen, like the app's QIDI Data frame: the four slots of a box with state
  (empty / loaded / feeding), slot number, material, colour, spool number, Spoolman vendor and remaining weight.
  With more than one printer a drop-down chooses the printer, with more than one box a second one the box. The
  data is read from the printer's Moonraker in the background (QIDI Plus4 / Max4; the Q2 is not supported) and
  refreshed with the refresh button, on a change of printer or box and when leaving the settings.
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
