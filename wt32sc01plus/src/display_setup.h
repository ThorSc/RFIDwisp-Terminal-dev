#pragma once
#include <lvgl.h>

// Brings up the ST7796 panel + FT6336 touch over LovyanGFX and wires them
// into LVGL as a single display/input driver pair. Call once from setup().
void displaySetup();

// Must be called periodically (e.g. every loop() iteration) to keep LVGL's
// tick and task handler running.
void displayLoop();

// Screen sleep: after `minutes` without a touch the backlight is switched
// off; the next touch wakes it (and is not passed on to the UI, so waking
// the screen cannot press a button by accident). 0 disables sleeping.
void displaySetSleepTimeout(uint8_t minutes);

// Switches the backlight on (if the screen sleeps) and restarts the sleep
// timer, e.g. when a tag is put on the reader.
void displayWake();
