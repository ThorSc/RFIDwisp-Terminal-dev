# RFID Wisp Terminal

Standalone firmware for the WT32-SC01 Plus (ESP32-S3, 3.5" touch display)
plus an RC522 RFID module (SPI): reads and writes the same QIDI spool tags as
the [RFIDwisp](https://github.com/ThorSc/RFIDwisp) PC app, shows what is in
the printer's QIDI box, and works with Spoolman - over Wi-Fi, no PC needed.

This repo holds **releases only** (prebuilt firmware binaries), published
here automatically when a version is released. The source is developed in
[`ThorSc/RFIDwisp-Terminal-dev`](https://github.com/ThorSc/RFIDwisp-Terminal-dev).

## Downloads

See the [latest release](../../releases/latest) for the current firmware
build. Each release contains:

| File | Purpose |
|------|---------|
| `RFIDwisp-ESP32-wt32sc01plus-vX.Y.Z.zip` | Everything for a first flash: `bootloader.bin`, `partitions.bin`, `boot_app0.bin`, `firmware.bin`, `FLASHING.md` (the exact `esptool.py` command and flash offsets), `lang.example.json`, the changelog and the licenses. |
| `RFIDwisp-ESP32-wt32sc01plus-firmware.bin` | Only the firmware. This is what the terminal downloads when you update it from its settings; you can also flash it over the network (OTA). |

## What it does

- **Tags:** put a tag on the reader and the terminal reads it and opens the
  edit screen with its data. "Edit tag" opens the same screen with empty
  fields to write a new tag (QIDI MIFARE Classic 1K, 16-byte spool payload).
- **QIDI box overview:** the main screen shows the four slots of a box of the
  selected printer - state, material, colour, spool number, Spoolman vendor
  and remaining weight - read from the printer's Moonraker (QIDI Plus4 and
  Max4). With several printers or boxes, drop-downs choose them.
- **Spoolman:** pick an existing spool or filament (shown with its colour) or
  create a new spool; the tag is written with the matching material, colour,
  vendor, spool number and weight.
- **Settings** (tabs *General*, *WiFi*, *Printer*, *Spoolman*): screen sleep,
  firmware update from the releases here, language (English or German),
  Wi-Fi status and reconfiguration, the list of printers, the Spoolman
  address.
- **Network:** Wi-Fi and the Spoolman address are set up through a captive
  portal on first boot. Once on Wi-Fi, the device advertises itself as
  `RFIDwisp-mobile` and accepts firmware updates over the network (OTA) - no
  USB cable needed after the first flash.

## Wiring and setup

See `FLASHING.md` in each release zip for flashing, and the source
repository's README for wiring (display/touch/RC522 pin assignments),
first-boot Wi-Fi setup and OTA updates.
