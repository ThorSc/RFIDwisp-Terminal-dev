# Third-party licenses

This firmware (`wt32sc01plus/`) is built with [PlatformIO](https://platformio.org/)
on the Arduino framework for ESP32 and pulls in the libraries listed in
`wt32sc01plus/platformio.ini` (`lib_deps`). All of them end up statically
linked into the firmware binary. This file lists each one, its license, and
what that means for redistributing the built firmware.

| Component | Copyright | License |
|---|---|---|
| [Arduino core for ESP32](https://github.com/espressif/arduino-esp32) (Espressif Systems) | (c) Espressif Systems and contributors | LGPL-2.1-or-later |
| [Arduino_MFRC522v2](https://github.com/OSSLibraries/Arduino_MFRC522v2) | (c) OSSLibraries contributors | LGPL-2.1 |
| [LVGL](https://github.com/lvgl/lvgl) | (c) 2021 LVGL Kft | MIT |
| [Montserrat](https://github.com/JulietaUla/Montserrat) (font, `src/fonts/Montserrat-Medium.ttf`, as shipped with LVGL) | (c) 2011 The Montserrat Project Authors | SIL Open Font License 1.1 |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | (c) 2014-2024 Benoit Blanchon | MIT |
| [WiFiManager](https://github.com/tzapu/WiFiManager) | (c) 2015 tzapu | MIT |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | (c) 2020 lovyan03; includes code from Adafruit_ILI9341 | MIT and BSD-2-Clause (FreeBSD License) |

## LGPL-2.1 components (Arduino-ESP32 core, Arduino_MFRC522v2)

These two are statically linked into `firmware.bin`, which LGPL-2.1 permits
for a device like this as long as the complete source needed to modify the
LGPL component and relink/rebuild the whole firmware is available - which it
is: this repository is public and `pio run` reproduces the exact build,
pinned to the library versions in `platformio.ini`/`platformio.lock`. If you
redistribute a modified build, keep that source available under the same
terms. Full license text: <https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html>.

## MIT components (LVGL, ArduinoJson, WiFiManager, part of LovyanGFX)

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

## BSD-2-Clause / FreeBSD License component (part of LovyanGFX)

```
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
