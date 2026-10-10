# RFIDwisp on WT32-SC01 Plus

## Downloads

Prebuilt firmware is published as a release to the distribution repo
[`ThorSc/RFIDwisp-Terminal`](https://github.com/ThorSc/RFIDwisp-Terminal/releases/latest)
whenever a push to `master` carries a new `VERSION` (see "Releasing" below).
This repo is the source those releases are built from (see "License" below
for why it's public: two statically-linked dependencies are LGPL-2.1).

Standalone firmware for the WT32-SC01 Plus (ESP32-S3, 3.5" ST7796 touch
display) plus an external RC522 RFID module (SPI): a from-scratch, feature-equivalent
reimplementation of the [Flutter RFIDwisp app](../RFIDwisp-dev) that runs
without a PC, talking to Spoolman over Wi-Fi.

This is a reimplementation, not a port: Flutter cannot run on a bare
microcontroller (no Linux/GPU), so the UI is built with LVGL. The tag format
logic (`src/qidi_tag.h/.cpp`) is a manual C++ port of `filament_data.dart` /
`filament_spool.dart` and must be kept in sync if that format ever changes.

Being built feature by feature (see "Status" below); this is not a 1:1 UI
clone yet.

## Project layout

```
wt32sc01plus/
  platformio.ini        PlatformIO project (ESP32-S3, Arduino framework)
  include/lv_conf.h      LVGL configuration
  data/lang.example.json  Example UI string override (see "Language")
  src/
    board_config.h       Display/touch/RC522 pin assignments - check first
    display_setup.*       LovyanGFX + LVGL bring-up for the ST7796/FT6336
    mfrc522_reader.*        RC522 wrapper: find tag, authenticate, read/write
    qidi_tag.*              Material/colour/vendor tables + 16-byte encoding
    settings.*               Spoolman/OTA settings, in NVS
    strings.*                 UI text lookup with optional LittleFS override
    wifi_setup.*               Wi-Fi captive-portal provisioning
    ota_setup.*                 Wi-Fi (OTA) firmware updates
    updater.*                   Self-update from the GitHub releases
    ui.*                        LVGL screens (main, spool edit, settings)
    main.cpp                     Wires it all together
```

## Bill of materials

The parts list for building the terminal is in [`CAD/BOM.md`](CAD/BOM.md).

## Wiring

- **Display + touch**: on-board. The ST7796 display uses an 8-bit parallel
  (i80) bus, not SPI; the pins in `board_config.h` are verified against the
  board's datasheet and confirmed working on hardware.
- **RC522**: external module in SPI mode, wired to a *dedicated* SPI bus
  separate from the on-board display's i80 bus. Defaults in `board_config.h`
  (expansion header pins; GPIO 26-32 cannot be used on the ESP32-S3 -
  flash/PSRAM):
  - `RC522_SCK` = GPIO 12, `RC522_MISO` = GPIO 13, `RC522_MOSI` = GPIO 11,
    `RC522_SS` = GPIO 10
  - `RC522_RST` = GPIO 14, or tie the module's RST pin to 3.3V and set
    `RC522_RST` to `-1` if you don't want to wire it up.

## Building

Requires [PlatformIO](https://platformio.org/) (CLI or the VS Code
extension).

```
cd wt32sc01plus
pio run                # build
pio run -t upload      # flash over USB
pio run -t uploadfs    # flash data/ to LittleFS (only needed for a lang.json override)
pio device monitor      # serial log (115200 baud)
```

Once a device is on the network, later builds can also be flashed over Wi-Fi
instead of USB (see "OTA updates" below):

```
pio run -t upload --upload-port RFIDwisp-mobile.local
```

## First boot / Wi-Fi setup

On first boot (or after "Reconfigure network" in Settings), the device opens
a Wi-Fi access point named **RFIDwisp-Setup**. Join it from a phone or laptop;
a captive-portal page should open automatically (or browse to
`http://192.168.4.1`). Enter:

- your home Wi-Fi SSID and password,
- **Spoolman address** (e.g. `http://spoolman.local:7912`; leave empty to
  skip Spoolman - it can be turned back on/off later in Settings without
  re-running the portal).

The device reboots into your network. Wi-Fi credentials are stored by the
WiFiManager library in its own NVS namespace; the Spoolman
settings are stored separately (see `settings.h`) and are not erased by
"Reconfigure network" (only the Wi-Fi credentials are).

## OTA updates

Once connected to Wi-Fi, the device advertises itself as **RFIDwisp-mobile**
(hostname/mDNS name) and accepts firmware uploads over the network, so
later updates don't need a USB cable:

```
cd wt32sc01plus
pio run -t upload --upload-port RFIDwisp-mobile.local
```

This also works from the Arduino IDE: `RFIDwisp-mobile` should show up under
**Tools > Port** as a network port once the device is on the same network.

By default the OTA port has no password. To require one, set an "OTA update
password" in the Wi-Fi setup portal (join **RFIDwisp-Setup**, or run
"Reconfigure network" in Settings) alongside the Spoolman address; leave it
empty to keep OTA open. The password is stored in NVS (`settings.h`) like the
Spoolman address, and PlatformIO/Arduino will prompt for it on upload once
one is set.

## Updating from GitHub

At startup (if Wi-Fi is connected) the device checks the latest release of
[`ThorSc/RFIDwisp-Terminal`](https://github.com/ThorSc/RFIDwisp-Terminal/releases/latest)
and, if it is newer than the running firmware, asks whether to install it.
**Settings** has the same: a "Check at startup" switch and a button that checks
right away and then installs the version it found. The device downloads the
release's `RFIDwisp-ESP32-wt32sc01plus-firmware.bin` over TLS (certificates are
validated, the clock is set via NTP), verifies it against the SHA-256 GitHub
publishes and only then activates it and restarts. A failed or interrupted
download leaves the running firmware untouched. Releases published before this feature have no
such file and can only be flashed over USB/OTA as above; the first
self-updating version has to be flashed that way too.

## Language

All UI text goes through `T(StrId::...)` (see `src/strings.h`), never a
literal string, specifically so a language can be swapped without touching
UI code. English and German are built into the firmware (`src/strings.cpp`);
the language is chosen in Settings > General (a list with flags, applied by a
restart) and stored on the device. The texts are drawn with Montserrat from a
TrueType file in flash (`src/fonts/`), because LVGL's built-in fonts have no
umlauts. To change single texts, to try another language or to shrink the
string table if flash is tight, an override file replaces the built-in text of
whichever language is chosen:

1. Copy `data/lang.example.json` to `data/lang.json` and translate the
   values (keep the keys unchanged; an entry can be omitted to keep the
   built-in text for that string). Characters outside Latin-1 (e.g. Cyrillic)
   are not in the font yet.
2. `pio run -t uploadfs` to write it to the device's LittleFS partition.

No firmware rebuild needed to change or remove the override afterwards.

## What it does so far

- **Edit tag**: a tag put on the reader while the main screen is showing is
  read automatically. The "Edit tag" button opens the edit screen with empty
  fields, with or without a tag on the reader. Reading authenticates sector 1 with the same
  default-key list as the Flutter app (`FF FF FF FF FF FF`, all-zero, the
  MAD key, then `B0 B1 B2 B3 B4 B5`, each tried as key A then key B), reads
  the 16-byte payload from block 4, and shows it on the spool edit screen
  (material, colour, vendor, spool number, weight as dropdowns/spinboxes). A
  blank tag, or one without valid spool data, opens the empty edit screen.
- **Write tag** (edit screen): writes the encoded 16 bytes to block 4 and
  reads them back to confirm. **Read tag** on the same screen reads another
  tag without going back to the main screen.
- **QIDI box overview** on the main screen: the four slots of a box of the
  selected printer (state, slot, material, colour, spool number, Spoolman
  vendor, remaining weight), read from Moonraker's `multi_color_controller`
  and the `rfid_bridge` module. Printer and box drop-downs appear if there is
  more than one of each. The QIDI Q2 (no `multi_color_controller`) is not
  supported.
- **Settings screen**, in tabs like the app: *General* (screen sleep, check
  for firmware updates, firmware version), *WiFi* (status, "Reconfigure
  network"), *Printer* (printers with name and Moonraker address) and
  *Spoolman* (address, "Use Spoolman").
- **Spoolman-integrated write/edit screen**: when Spoolman is active, the
  edit screen's Vendor + Spool number fields are replaced by a **Spool**
  selector ("New spool" + every cached Spoolman spool) and a **Spoolman
  vendor** dropdown, mirroring `rfid_tag_panel.dart`'s state machine:
  - Reading a tag tries to match its spool number against a cached Spoolman
    spool and points the Spool selector at it; otherwise it falls back to
    "New spool" with the tag's own fields (matches the Flutter app's
    behaviour for a tag written by something else, or before Spoolman knew
    about that spool number).
  - Picking an existing spool fills Material/Colour (best fuzzy match, ports
    of `closestQidiColor`/`qidiMaterialChoices`), Spoolman vendor and Weight
    from it and disables those fields (read-only, like the Flutter app).
  - Writing with "New spool" selected looks up a matching Spoolman filament
    (vendor + material + colour) and creates the spool via `POST
    /api/v1/spool`, using the new id as the tag's spool number; if no
    filament matches, or the create call fails, it falls back to a plain
    (Spoolman-unlinked) tag write and reports that in the status line -
    same "skip, don't fail" behaviour as `_createSpoolmanSpool`.

- **Filament dropdown** on the edit screen (Spoolman mode, "New spool" only):
  picking one of the cached Spoolman filaments prefills Material/Colour/
  Spoolman vendor from it (weight is still entered manually) - mirrors
  `rfid_tag_panel.dart`'s second dropdown for reusing an existing filament
  when creating a spool.

The UI runs in landscape (480x320).

Deliberately out of scope (per product decision): Moonraker / printer
integration (the Flutter app's QIDI Data panel and printer management), QR
code / PDF label printing (`qr_code_panel.dart`, `label_pdf.dart`,
`label_output.dart` - no printer or file export on this device), the
desktop window auto-sizer, and the GitHub-release update checker.

## Known limitations of the current state

- RC522 access and the Wi-Fi portal are synchronous: the touchscreen is
  unresponsive while either runs. Fine for the explicit, occasional actions
  they're used for so far; a background FreeRTOS task would be needed before
  adding anything longer-running.
- Display flashed and confirmed on hardware; RC522 wiring, touch and the
  Wi-Fi/Spoolman flows are not yet tested on the device - review pin
  assignments and library versions before relying on it.

## Releasing

Releases are published to the public [`ThorSc/RFIDwisp-Terminal`](https://github.com/ThorSc/RFIDwisp-Terminal)
repo by [`.github/workflows/release.yml`](.github/workflows/release.yml),
mirroring the pattern used by the sibling Flutter project's
`RFIDwisp-dev`/`RFIDwisp` repos: this source repo stays private, only built
firmware is published.

1. Bump the version in [`VERSION`](VERSION).
2. Push to `master`. If that version has no release yet in the distribution
   repo, the workflow builds the firmware and publishes it as a GitHub
   Release there (source commit tagged `v<version>` here too); if it already
   does, the run is a no-op.
3. To rebuild/re-upload an already-released version (e.g. after fixing a
   packaging issue), run the workflow manually from the Actions tab with
   "rebuild" ticked.

One-time setup this repo's Settings -> Secrets and variables -> Actions
needs before the first release: a secret named `RELEASES_REPO_TOKEN`
holding a classic GitHub PAT with the `public_repo` scope (see the comment
at the top of `release.yml` for why classic, not fine-grained).

## License

MIT, see [LICENSE](LICENSE). Bundled/linked third-party libraries keep their
own licenses (two are LGPL-2.1, statically linked) - see
[THIRD-PARTY-LICENSES.md](THIRD-PARTY-LICENSES.md).
