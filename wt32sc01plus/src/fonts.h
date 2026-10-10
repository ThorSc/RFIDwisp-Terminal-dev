#pragma once
#include <lvgl.h>

// The UI fonts: Montserrat, the same face as LVGL's built-in fonts, but
// rendered from the TrueType file in flash (src/fonts/Montserrat-Medium.ttf),
// because the built-in fonts only have the characters from space to "~" and
// no umlauts. The symbols (LV_SYMBOL_*) come from the built-in fonts, which
// are the fallback of these ones.
extern const lv_font_t *fontSmall;  // 14 px, the default of all widgets
extern const lv_font_t *fontMedium; // 18 px
extern const lv_font_t *fontLarge;  // 24 px

// Creates the fonts. Call after the display is registered and before any
// screen is built.
void fontsInit();

// Makes fontSmall the text font of [screen] and so of everything on it that
// sets no font of its own (LVGL takes the default font from its compile-time
// setting, not from the theme).
void fontsApply(lv_obj_t *screen);
