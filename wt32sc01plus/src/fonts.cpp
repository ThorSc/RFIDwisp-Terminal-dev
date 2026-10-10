#include "fonts.h"

extern const uint8_t ttfStart[] asm("_binary_src_fonts_Montserrat_Medium_ttf_start");
extern const uint8_t ttfEnd[] asm("_binary_src_fonts_Montserrat_Medium_ttf_end");

const lv_font_t *fontSmall = &lv_font_montserrat_14;
const lv_font_t *fontMedium = &lv_font_montserrat_18;
const lv_font_t *fontLarge = &lv_font_montserrat_24;

// Glyph bitmap cache of each font, in bytes.
static const size_t kGlyphCacheBytes = 16 * 1024;

static const lv_font_t *makeFont(int pixels, const lv_font_t *builtIn) {
  lv_font_t *font = lv_tiny_ttf_create_data_ex(ttfStart, ttfEnd - ttfStart, pixels, kGlyphCacheBytes);
  if (!font) return builtIn; // out of memory: ASCII only, but it works
  font->fallback = builtIn;  // the symbols
  return font;
}

void fontsInit() {
  fontSmall = makeFont(14, &lv_font_montserrat_14);
  fontMedium = makeFont(18, &lv_font_montserrat_18);
  fontLarge = makeFont(24, &lv_font_montserrat_24);

  // The overlays (dialogs, "please wait", pickers) are children of the top layer.
  fontsApply(lv_layer_top());
}

void fontsApply(lv_obj_t *screen) { lv_obj_set_style_text_font(screen, fontSmall, 0); }
