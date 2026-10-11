#include "ui.h"
#include "display_setup.h"
#include "fonts.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
#include "board_config.h"
#include "settings.h"
#include "strings.h"
#include <Arduino.h>
#include <algorithm>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

// Implemented in main.cpp: performs the actual RC522 read/write and calls
// uiSetStatus() / uiShowSpool() with the result.
void handleReadTagRequested();
void handleEditTagRequested();
void handleBoxPrinterSelected(int index);
void handleBoxSelected(int box);
void handleBoxRefreshRequested();
void handleSettingsClosed();
void handleLanguageSelected(const char *code);
void handleWriteTagRequested();
void handleUpdateButtonPressed();
void handleInstallUpdateRequested();

// Implemented in wifi_setup.cpp.
void wifiSetupReset();

static lv_obj_t *scrMain;
static lv_obj_t *lblStatus;

// QIDI box overview on the main screen
struct SlotRow {
  lv_obj_t *dot, *lblSlot, *lblMaterial, *lblQidiVendor, *swatch, *lblNumber, *lblVendor, *lblWeight;
};
static lv_obj_t *ddMainPrinter, *lblMainPrinter, *ddMainBox, *lblBoxState, *btnBoxRefresh;
static SlotRow slotRows[4];

static lv_obj_t *scrEdit;
static lv_obj_t *ddMaterial;
static lv_obj_t *ddColor;
static lv_obj_t *lblRow3, *lblSpoolRow;
static lv_obj_t *ddVendor;       // row 3, non-Spoolman
static lv_obj_t *ddSpool;         // row 3, Spoolman
static lv_obj_t *sbSpoolNumber; // row 4, non-Spoolman
static lv_obj_t *ddSpoolmanVendor; // row 4, Spoolman
static lv_obj_t *sbWeight;
static lv_obj_t *lblFilament, *ddFilament; // Spoolman mode, "New spool" only
static lv_obj_t *btnColor, *lblColorHex; // shows the selected colour; ddColor is hidden state
static lv_obj_t *colorOverlay;
static lv_obj_t *lblEditStatus;
static lv_obj_t *sbSleep; // settings: screen sleep timeout in minutes
static lv_obj_t *btnSpool, *spoolSwatch, *spoolBtnLabel;          // Spoolman mode; ddSpool is hidden state
static lv_obj_t *btnFilament, *filamentSwatch, *filamentBtnLabel; // ditto for ddFilament
static lv_obj_t *pickerOverlay;
static bool pickerForSpool;

static bool spoolmanModeActive = false;
static std::vector<SpoolmanVendor> uiVendors;
static std::vector<SpoolmanSpool> uiSpools;
static std::vector<SpoolmanFilament> uiFilaments;

static lv_obj_t *scrSettings;
static lv_obj_t *lblWifiStatus;
static lv_obj_t *taSpoolmanAddress;
static lv_obj_t *ddPrinter, *taPrinterName, *taPrinterAddress, *btnPrinterDelete, *lblPrinterError;
static int selectedPrinter = -1; // index into settings.printers, -1 = none
static lv_obj_t *keyboard;
static lv_obj_t *swUseSpoolman;
static lv_obj_t *swUpdateCheck;
static lv_obj_t *lblUpdateStatus, *lblUpdateButton;
static lv_obj_t *busyOverlay, *busyLabel, *busyBar;



static void buildDropdownOptions(lv_obj_t *dd, const NamedByte *table, size_t count) {
  String options;
  for (size_t i = 0; i < count; i++) {
    if (i > 0) options += "\n";
    options += table[i].name;
  }
  lv_dropdown_set_options(dd, options.c_str());
}

static int selectedCode(lv_obj_t *dd, const NamedByte *table, size_t count) {
  uint16_t index = lv_dropdown_get_selected(dd);
  if (index >= count) return table[0].code;
  return table[index].code;
}

static void selectCode(lv_obj_t *dd, const NamedByte *table, size_t count, uint8_t code) {
  for (size_t i = 0; i < count; i++) {
    if (table[i].code == code) {
      lv_dropdown_set_selected(dd, i);
      return;
    }
  }
  lv_dropdown_set_selected(dd, 0);
}

static uint32_t rgbFromHex(const char *hex) {
  if (hex[0] == '#') hex++;
  return (uint32_t)strtoul(hex, nullptr, 16);
}

static lv_color_t contrastingText(uint32_t rgb) {
  uint32_t r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
  return (299 * r + 587 * g + 114 * b) > 128000 ? lv_color_black() : lv_color_white();
}

static void updateColorButton() {
  uint16_t index = lv_dropdown_get_selected(ddColor);
  if (index >= qidiColorsCount) index = 0;
  uint32_t rgb = rgbFromHex(qidiColors[index].name);
  lv_obj_set_style_bg_color(btnColor, lv_color_hex(rgb), 0);
  lv_obj_set_style_text_color(lblColorHex, contrastingText(rgb), 0);
  lv_label_set_text(lblColorHex, qidiColors[index].name);
}

static void selectColorCode(uint8_t code) {
  selectCode(ddColor, qidiColors, qidiColorsCount, code);
  updateColorButton();
}

static void closeColorOverlay() {
  if (colorOverlay) {
    lv_obj_del_async(colorOverlay);
    colorOverlay = nullptr;
  }
}

static void colorSwatchCb(lv_event_t *e) {
  uintptr_t index = (uintptr_t)lv_event_get_user_data(e);
  lv_dropdown_set_selected(ddColor, (uint16_t)index);
  updateColorButton();
  closeColorOverlay();
}

static void openColorOverlay(lv_event_t *e) {
  if (colorOverlay) return;
  colorOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(colorOverlay, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_align(colorOverlay, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(colorOverlay, lv_color_hex(0x202020), 0);
  lv_obj_set_style_bg_opa(colorOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(colorOverlay, 0, 0);
  lv_obj_set_style_radius(colorOverlay, 0, 0);
  lv_obj_set_style_pad_all(colorOverlay, 0, 0);
  lv_obj_clear_flag(colorOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(
      colorOverlay,
      [](lv_event_t *ev) {
        if (lv_event_get_target(ev) == lv_event_get_current_target(ev)) closeColorOverlay();
      },
      LV_EVENT_CLICKED, nullptr);

  const int cols = 6, cellW = 70, cellH = 64;
  const int originX = (SCREEN_WIDTH - cols * cellW) / 2;
  const int rows = (qidiColorsCount + cols - 1) / cols;
  const int originY = (SCREEN_HEIGHT - rows * cellH) / 2;
  for (size_t i = 0; i < qidiColorsCount; i++) {
    uint32_t rgb = rgbFromHex(qidiColors[i].name);
    lv_obj_t *btn = lv_btn_create(colorOverlay);
    lv_obj_set_size(btn, cellW - 8, cellH - 8);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, originX + (i % cols) * cellW + 4,
                 originY + (i / cols) * cellH + 4);
    lv_obj_set_style_bg_color(btn, lv_color_hex(rgb), 0);
    lv_obj_add_event_cb(btn, colorSwatchCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, qidiColors[i].name);
    lv_obj_set_style_text_color(label, contrastingText(rgb), 0);
    lv_obj_center(label);
  }
}

static void readButtonCb(lv_event_t *e) { handleReadTagRequested(); }

static void editButtonCb(lv_event_t *e) { handleEditTagRequested(); }

static void backButtonCb(lv_event_t *e) { lv_scr_load(scrMain); }

static void saveButtonCb(lv_event_t *e) { handleWriteTagRequested(); }

static void setNumberField(lv_obj_t *field, long value) {
  lv_textarea_set_text(field, String(value).c_str());
}

static long getNumberField(lv_obj_t *field) {
  return atol(lv_textarea_get_text(field));
}

static lv_obj_t *numberOverlay, *numberInput, *numberTarget;
static long numberMax;

static void closeNumberOverlay() {
  if (numberOverlay) {
    lv_obj_del_async(numberOverlay);
    numberOverlay = nullptr;
    numberTarget = nullptr;
  }
}

static void numberKeyboardCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    const char *text = lv_textarea_get_text(numberInput);
    if (text[0] != '\0' && numberTarget) {
      long value = atol(text);
      if (value > numberMax) value = numberMax;
      setNumberField(numberTarget, value);
      if (numberTarget == sbSleep) {
        settings.sleepMinutes = (uint8_t)value;
        settings.save();
        displaySetSleepTimeout(settings.sleepMinutes);
      }
    }
    closeNumberOverlay();
  } else if (code == LV_EVENT_CANCEL) {
    closeNumberOverlay();
  }
}

// Tapping a number field opens a full-screen numeric keypad (a spinbox has no
// keyboard, and the normal keyboard would cover the fields at the bottom).
static void openNumberOverlay(lv_event_t *e) {
  lv_obj_t *target = (lv_obj_t *)lv_event_get_user_data(e);
  if (numberOverlay || lv_obj_has_state(target, LV_STATE_DISABLED)) return;
  numberTarget = target;
  bool isWeight = target == sbWeight;
  bool isSleep = target == sbSleep;
  numberMax = isWeight ? 10000 : isSleep ? 60 : 999;

  numberOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(numberOverlay, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_align(numberOverlay, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(numberOverlay, lv_color_hex(0x202020), 0);
  lv_obj_set_style_bg_opa(numberOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(numberOverlay, 0, 0);
  lv_obj_set_style_radius(numberOverlay, 0, 0);
  lv_obj_set_style_pad_all(numberOverlay, 0, 0);
  lv_obj_clear_flag(numberOverlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(numberOverlay);
  lv_label_set_text(title, isWeight ? T(StrId::Weight) : isSleep ? T(StrId::SleepTimeout) : T(StrId::SpoolNumber));
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 15, 8);

  numberInput = lv_textarea_create(numberOverlay);
  lv_textarea_set_one_line(numberInput, true);
  lv_textarea_set_accepted_chars(numberInput, "0123456789");
  lv_textarea_set_max_length(numberInput, isWeight ? 5 : isSleep ? 2 : 3);
  lv_textarea_set_placeholder_text(numberInput, String(getNumberField(target)).c_str());
  lv_obj_set_size(numberInput, SCREEN_WIDTH - 30, 46);
  lv_obj_align(numberInput, LV_ALIGN_TOP_LEFT, 15, 30);

  lv_obj_t *keyboardWidget = lv_keyboard_create(numberOverlay);
  lv_keyboard_set_mode(keyboardWidget, LV_KEYBOARD_MODE_NUMBER);
  lv_obj_set_size(keyboardWidget, SCREEN_WIDTH, 230);
  lv_obj_align(keyboardWidget, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(keyboardWidget, numberInput);
  lv_obj_add_event_cb(keyboardWidget, numberKeyboardCb, LV_EVENT_ALL, nullptr);
}

static lv_obj_t *makeNumberField(lv_obj_t *parent, int x, int y, int width) {
  lv_obj_t *field = lv_textarea_create(parent);
  lv_textarea_set_one_line(field, true);
  lv_textarea_set_text(field, "0");
  lv_obj_set_size(field, width, 40);
  lv_obj_align(field, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_add_event_cb(field, openNumberOverlay, LV_EVENT_CLICKED, field);
  return field;
}

static void setNewSpoolFieldsEnabled(bool enabled) {
  auto set = [enabled](lv_obj_t *obj) {
    if (enabled) {
      lv_obj_clear_state(obj, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
  };
  set(ddMaterial);
  set(btnColor);
  set(ddSpoolmanVendor);
  set(btnFilament);
}

static void spoolDropdownCb(lv_event_t *e);
static void filamentDropdownCb(lv_event_t *e);

static bool parseColorHex(const String &hex, uint32_t &rgb) {
  String h = hex;
  if (h.startsWith("#")) h.remove(0, 1);
  if (h.length() < 6) return false;
  for (int i = 0; i < 6; i++) {
    if (!isxdigit((unsigned char)h[i])) return false;
  }
  rgb = (uint32_t)strtoul(h.substring(0, 6).c_str(), nullptr, 16);
  return true;
}

// Text and colour of entry `index` of the Spool (forSpool) or Filament
// picker; index 0 is "New spool" / "(pick to prefill)" and has no colour.
static void pickerEntry(bool forSpool, size_t index, String &text, bool &hasColor, uint32_t &rgb) {
  hasColor = false;
  if (index == 0) {
    text = forSpool ? T(StrId::NewSpool) : T(StrId::NoFilament);
    return;
  }
  if (forSpool) {
    if (index - 1 >= uiSpools.size()) return;
    const SpoolmanSpool &spool = uiSpools[index - 1];
    text = "#" + String(spool.id) + " " + spool.material;
    if (spool.vendorName.length()) text += " (" + spool.vendorName + ")";
    text += " - " + String(spool.remainingWeightGrams) + "g";
    hasColor = parseColorHex(spool.colorHex, rgb);
  } else {
    if (index - 1 >= uiFilaments.size()) return;
    const SpoolmanFilament &filament = uiFilaments[index - 1];
    text = filament.vendorName + " " + filament.material;
    hasColor = parseColorHex(filament.colorHex, rgb);
  }
}

static void paintSwatch(lv_obj_t *swatch, bool hasColor, uint32_t rgb) {
  lv_obj_set_style_bg_color(swatch, lv_color_hex(hasColor ? rgb : 0x707070), 0);
  lv_obj_set_style_bg_opa(swatch, hasColor ? LV_OPA_COVER : LV_OPA_40, 0);
}

static void updateSpoolButton() {
  String text;
  bool hasColor;
  uint32_t rgb = 0;
  pickerEntry(true, lv_dropdown_get_selected(ddSpool), text, hasColor, rgb);
  lv_label_set_text(spoolBtnLabel, text.c_str());
  paintSwatch(spoolSwatch, hasColor, rgb);
}

static void updateFilamentButton() {
  String text;
  bool hasColor;
  uint32_t rgb = 0;
  pickerEntry(false, lv_dropdown_get_selected(ddFilament), text, hasColor, rgb);
  lv_label_set_text(filamentBtnLabel, text.c_str());
  paintSwatch(filamentSwatch, hasColor, rgb);
}

static void closePicker() {
  if (pickerOverlay) {
    lv_obj_del_async(pickerOverlay);
    pickerOverlay = nullptr;
  }
}

static void pickerItemCb(lv_event_t *e) {
  uint16_t index = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
  if (pickerForSpool) {
    lv_dropdown_set_selected(ddSpool, index);
    spoolDropdownCb(nullptr);
  } else {
    lv_dropdown_set_selected(ddFilament, index);
    filamentDropdownCb(nullptr);
  }
  closePicker();
}

// One row/button: colour swatch on the left, text on the right.
static lv_obj_t *makeSwatchButton(lv_obj_t *parent, int width, int height, int textWidth,
                                  lv_obj_t **swatchOut, lv_obj_t **labelOut) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, width, height);

  lv_obj_t *swatch = lv_obj_create(btn);
  lv_obj_set_size(swatch, 24, 24);
  lv_obj_set_style_radius(swatch, 4, 0);
  lv_obj_set_style_border_width(swatch, 1, 0);
  lv_obj_set_style_border_color(swatch, lv_color_hex(0x909090), 0);
  lv_obj_set_style_pad_all(swatch, 0, 0);
  lv_obj_clear_flag(swatch, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(swatch, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *label = lv_label_create(btn);
  lv_obj_set_width(label, textWidth);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 32, 0);

  *swatchOut = swatch;
  *labelOut = label;
  return btn;
}

// Full-screen scrollable list where every entry shows its colour.
static void openPicker(bool forSpool) {
  if (pickerOverlay) return;
  pickerForSpool = forSpool;

  pickerOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(pickerOverlay, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_align(pickerOverlay, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(pickerOverlay, lv_color_hex(0x202020), 0);
  lv_obj_set_style_bg_opa(pickerOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(pickerOverlay, 0, 0);
  lv_obj_set_style_radius(pickerOverlay, 0, 0);
  lv_obj_set_style_pad_all(pickerOverlay, 0, 0);
  lv_obj_clear_flag(pickerOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(
      pickerOverlay,
      [](lv_event_t *ev) {
        if (lv_event_get_target(ev) == lv_event_get_current_target(ev)) closePicker();
      },
      LV_EVENT_CLICKED, nullptr);

  lv_obj_t *list = lv_obj_create(pickerOverlay);
  lv_obj_set_size(list, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 20);
  lv_obj_align(list, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(list, 4, 0);
  lv_obj_set_style_pad_row(list, 4, 0);

  size_t count = 1 + (forSpool ? uiSpools.size() : uiFilaments.size());
  for (size_t i = 0; i < count; i++) {
    String text;
    bool hasColor;
    uint32_t rgb = 0;
    pickerEntry(forSpool, i, text, hasColor, rgb);

    lv_obj_t *swatch, *label;
    lv_obj_t *row = makeSwatchButton(list, SCREEN_WIDTH - 44, 40, SCREEN_WIDTH - 100, &swatch, &label);
    lv_label_set_text(label, text.c_str());
    paintSwatch(swatch, hasColor, rgb);
    lv_obj_add_event_cb(row, pickerItemCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
  }
}

// A chosen filament fixes material, colour and vendor of the new spool, so
// only the initial weight stays editable; "(pick to prefill)" frees them again.
static void setFilamentFieldsEnabled(bool enabled) {
  lv_obj_t *fields[] = {ddMaterial, btnColor, ddSpoolmanVendor};
  for (lv_obj_t *field : fields) {
    if (enabled) {
      lv_obj_clear_state(field, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(field, LV_STATE_DISABLED);
    }
  }
}

static void filamentDropdownCb(lv_event_t *e) {
  updateFilamentButton();
  uint16_t index = lv_dropdown_get_selected(ddFilament);
  if (index == 0 || index - 1 >= uiFilaments.size()) {
    setFilamentFieldsEnabled(true); // "(pick to prefill)"
    return;
  }
  setFilamentFieldsEnabled(false);

  const SpoolmanFilament &filament = uiFilaments[index - 1];
  selectCode(ddMaterial, qidiMaterials, qidiMaterialsCount,
             closestQidiMaterialCode(filament.material.c_str()));
  selectColorCode(closestQidiColorCode(filament.colorHex.c_str()));
  for (size_t i = 0; i < uiVendors.size(); i++) {
    if (uiVendors[i].id == filament.vendorId) {
      lv_dropdown_set_selected(ddSpoolmanVendor, i);
      break;
    }
  }
}

static void spoolDropdownCb(lv_event_t *e) {
  updateSpoolButton();
  uint16_t index = lv_dropdown_get_selected(ddSpool);
  if (index == 0 || index - 1 >= uiSpools.size()) {
    setNewSpoolFieldsEnabled(true);
    setNumberField(sbSpoolNumber, 0);
    lv_dropdown_set_selected(ddFilament, 0);
    updateFilamentButton();
    return;
  }
  const SpoolmanSpool &spool = uiSpools[index - 1];
  setNumberField(sbSpoolNumber, spool.id);
  lv_dropdown_set_selected(ddFilament, 0);
  for (size_t i = 0; i < uiFilaments.size(); i++) {
    if (uiFilaments[i].id == spool.filamentId) {
      lv_dropdown_set_selected(ddFilament, i + 1);
      break;
    }
  }
  updateFilamentButton();
  selectCode(ddMaterial, qidiMaterials, qidiMaterialsCount,
             closestQidiMaterialCode(spool.material.c_str()));
  selectColorCode(closestQidiColorCode(spool.colorHex.c_str()));
  for (size_t i = 0; i < uiVendors.size(); i++) {
    if (uiVendors[i].id == spool.vendorId) {
      lv_dropdown_set_selected(ddSpoolmanVendor, i);
      break;
    }
  }
  setNumberField(sbWeight, spool.remainingWeightGrams);
  setNewSpoolFieldsEnabled(false);
}

static void loadPrinterTab();

static void refreshSettingsScreen() {
  setNumberField(sbSleep, settings.sleepMinutes);
  lv_textarea_set_text(taSpoolmanAddress, settings.spoolmanAddress.c_str());
  if (settings.useSpoolman) {
    lv_obj_add_state(swUseSpoolman, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(swUseSpoolman, LV_STATE_CHECKED);
  }
  loadPrinterTab();
}

static void settingsButtonCb(lv_event_t *e) {
  refreshSettingsScreen();
  lv_scr_load(scrSettings);
}

static void useSpoolmanToggleCb(lv_event_t *e) {
  settings.useSpoolman = lv_obj_has_state(swUseSpoolman, LV_STATE_CHECKED);
  settings.save();
}

static void updateCheckToggleCb(lv_event_t *e) {
  settings.checkForUpdates = lv_obj_has_state(swUpdateCheck, LV_STATE_CHECKED);
  settings.save();
}

static void updateButtonCb(lv_event_t *e) { handleUpdateButtonPressed(); }

static void updatePromptCb(lv_event_t *e) {
  lv_obj_t *box = lv_event_get_current_target(e);
  bool install = lv_msgbox_get_active_btn(box) == 0;
  lv_msgbox_close_async(box);
  if (install) handleInstallUpdateRequested();
}

void uiShowUpdatePrompt(const char *version) {
  // The button map is kept by the message box, so it has to outlive this call.
  static const char *buttons[3];
  buttons[0] = T(StrId::UpdateInstall);
  buttons[1] = T(StrId::UpdateLater);
  buttons[2] = "";

  String text = String(T(StrId::UpdateAvailable)) + " v" + version + "\n" + T(StrId::Firmware) +
                " v" + FIRMWARE_VERSION;
  lv_obj_t *box = lv_msgbox_create(nullptr, T(StrId::UpdateDialogTitle), text.c_str(), buttons, false);
  lv_obj_set_width(box, 380);
  lv_obj_center(box);
  lv_obj_add_event_cb(box, updatePromptCb, LV_EVENT_VALUE_CHANGED, nullptr);
}

void uiShowBusy(const char *text, int percent) {
  if (!busyOverlay) {
    busyOverlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(busyOverlay, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_align(busyOverlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(busyOverlay, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(busyOverlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(busyOverlay, 0, 0);
    lv_obj_set_style_radius(busyOverlay, 0, 0);
    lv_obj_clear_flag(busyOverlay, LV_OBJ_FLAG_SCROLLABLE);

    busyLabel = lv_label_create(busyOverlay);
    lv_obj_set_width(busyLabel, SCREEN_WIDTH - 60);
    lv_label_set_long_mode(busyLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(busyLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(busyLabel, lv_color_white(), 0);
    lv_obj_align(busyLabel, LV_ALIGN_CENTER, 0, -30);

    busyBar = lv_bar_create(busyOverlay);
    lv_obj_set_size(busyBar, SCREEN_WIDTH - 100, 16);
    lv_obj_align(busyBar, LV_ALIGN_CENTER, 0, 30);
  }
  lv_label_set_text(busyLabel, text);
  if (percent >= 0) {
    lv_obj_clear_flag(busyBar, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(busyBar, percent, LV_ANIM_OFF);
  } else {
    lv_obj_add_flag(busyBar, LV_OBJ_FLAG_HIDDEN);
  }
}

void uiHideBusy() {
  if (!busyOverlay) return;
  lv_obj_del(busyOverlay);
  busyOverlay = busyLabel = busyBar = nullptr;
}

void uiSetUpdateState(const char *status, const char *buttonText) {
  lv_label_set_text(lblUpdateStatus, status);
  lv_label_set_text(lblUpdateButton, buttonText);
}

static void saveSpoolmanAddress() {
  String address = lv_textarea_get_text(taSpoolmanAddress);
  address.trim();
  while (address.endsWith("/")) address.remove(address.length() - 1);
  if (address.length() && !address.startsWith("http://") && !address.startsWith("https://")) {
    address = "http://" + address;
  }
  lv_textarea_set_text(taSpoolmanAddress, address.c_str());
  settings.spoolmanAddress = address;
  settings.useSpoolman = address.length() > 0;
  if (settings.useSpoolman) {
    lv_obj_add_state(swUseSpoolman, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(swUseSpoolman, LV_STATE_CHECKED);
  }
  settings.save();
}

// Cleans up an address entered by the user, like normalizeAddress() in the
// Flutter app: a missing scheme becomes http://, trailing slashes go. Returns
// false if the result is not an http(s) address with a host.
static bool normalizeAddress(String &address) {
  address.trim();
  if (address.isEmpty()) return false;
  for (size_t i = 0; i < address.length(); i++) {
    if (isspace((unsigned char)address[i])) return false;
  }
  if (address.indexOf("://") < 0) address = "http://" + address;
  while (address.endsWith("/")) address.remove(address.length() - 1);
  int hostStart = address.startsWith("https://") ? 8 : address.startsWith("http://") ? 7 : -1;
  return hostStart > 0 && (int)address.length() > hostStart && address[hostStart] != '/';
}

static String printerListName(const Printer &printer) {
  return printer.name.length() ? printer.name : printer.id;
}

// Fills the dropdown from settings.printers and shows/hides the fields below.
static void refreshPrinterWidgets() {
  String options;
  for (size_t i = 0; i < settings.printers.size(); i++) {
    if (i > 0) options += "\n";
    options += printerListName(settings.printers[i]);
  }
  bool any = selectedPrinter >= 0 && selectedPrinter < (int)settings.printers.size();
  if (settings.printers.empty()) {
    lv_dropdown_set_options(ddPrinter, T(StrId::PrinterNone));
    lv_obj_add_state(ddPrinter, LV_STATE_DISABLED);
  } else {
    lv_dropdown_set_options(ddPrinter, options.c_str());
    lv_obj_clear_state(ddPrinter, LV_STATE_DISABLED);
    if (any) lv_dropdown_set_selected(ddPrinter, selectedPrinter);
  }
  for (lv_obj_t *obj : {taPrinterName, taPrinterAddress}) {
    if (any) {
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (any) {
    lv_obj_clear_state(btnPrinterDelete, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(btnPrinterDelete, LV_STATE_DISABLED);
    lv_label_set_text(lblPrinterError, "");
  }
}

// Makes [index] the printer shown in the form (-1 clears the form).
static void selectPrinter(int index) {
  selectedPrinter = (index >= 0 && index < (int)settings.printers.size()) ? index : -1;
  const Printer *printer = selectedPrinter >= 0 ? &settings.printers[selectedPrinter] : nullptr;
  lv_textarea_set_text(taPrinterName, printer ? printer->name.c_str() : "");
  lv_textarea_set_text(taPrinterAddress, printer ? printer->address.c_str() : "");
  lv_label_set_text(lblPrinterError, "");
  refreshPrinterWidgets();
}

// Entries that were never filled in completely are not stored (see
// AppSettings::save), so they are dropped when the screen is left.
static void dropIncompletePrinters() {
  for (size_t i = settings.printers.size(); i-- > 0;) {
    const Printer &printer = settings.printers[i];
    if (printer.name.isEmpty() || printer.address.isEmpty()) {
      settings.printers.erase(settings.printers.begin() + i);
    }
  }
  selectedPrinter = -1;
}

static void loadPrinterTab() {
  dropIncompletePrinters();
  selectPrinter(settings.printers.empty() ? -1 : 0);
}

// Writes the form back into the selected printer. A blank form is not an
// error; an address that is not valid is not stored and reported instead.
static void commitPrinterFields() {
  if (selectedPrinter < 0 || selectedPrinter >= (int)settings.printers.size()) return;
  Printer &printer = settings.printers[selectedPrinter];

  String name = lv_textarea_get_text(taPrinterName);
  name.trim();
  String address = lv_textarea_get_text(taPrinterAddress);
  address.trim();
  bool blank = name.isEmpty() && address.isEmpty();

  bool addressOk = address.isEmpty() || normalizeAddress(address);
  printer.name = name;
  if (address.isEmpty()) {
    printer.address = "";
  } else if (addressOk) {
    printer.address = address;
    lv_textarea_set_text(taPrinterAddress, address.c_str());
  }

  const char *error = "";
  if (!blank && name.isEmpty()) {
    error = T(StrId::FieldRequired);
  } else if (!blank && (!addressOk || address.isEmpty())) {
    error = T(StrId::AddressInvalid);
  }
  lv_label_set_text(lblPrinterError, error);
  settings.save();

  // The name may have changed; keep the list entry in step.
  refreshPrinterWidgets();
}

static void printerSelectedCb(lv_event_t *e) {
  // Read the choice first: committing refreshes the dropdown, which resets it
  // to the previously selected printer.
  int chosen = lv_dropdown_get_selected(ddPrinter);
  commitPrinterFields();
  selectPrinter(chosen);
}

static void printerNewCb(lv_event_t *e) {
  commitPrinterFields();
  if (settings.printers.size() >= kMaxPrinters) return;
  String id;
  for (int number = 1;; number++) {
    id = "printer" + String(number);
    bool used = false;
    for (const Printer &printer : settings.printers) used = used || printer.id == id;
    if (!used) break;
  }
  settings.printers.push_back(Printer{id, "", ""});
  selectPrinter(settings.printers.size() - 1);
}

static void printerDeleteCb(lv_event_t *e) {
  if (selectedPrinter < 0 || selectedPrinter >= (int)settings.printers.size()) return;
  settings.printers.erase(settings.printers.begin() + selectedPrinter);
  settings.save();
  selectPrinter(settings.printers.empty() ? -1 : 0);
}

// Commits the text field that was being edited.
static void commitSettingsField(lv_obj_t *field) {
  if (field == taSpoolmanAddress) {
    saveSpoolmanAddress();
  } else if (field == taPrinterName || field == taPrinterAddress) {
    commitPrinterFields();
  }
}

static void settingsFieldFocusCb(lv_event_t *e) {
  lv_keyboard_set_textarea(keyboard, lv_event_get_target(e));
  lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void settingsFieldDefocusCb(lv_event_t *e) {
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  commitSettingsField(lv_event_get_target(e));
}

static void keyboardReadyCb(lv_event_t *e) {
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  commitSettingsField(lv_keyboard_get_textarea(keyboard));
}

static void settingsBackCb(lv_event_t *e) {
  commitSettingsField(lv_keyboard_get_textarea(keyboard));
  dropIncompletePrinters();
  lv_scr_load(scrMain);
  handleSettingsClosed();
}

static void reconfigureNetworkCb(lv_event_t *e) { wifiSetupReset(); }

static lv_obj_t *labeledNumberField(lv_obj_t *parent, const char *labelText, int x, int y) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, labelText);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y);
  return makeNumberField(parent, x, y + 17, 140);
}

static void boxPrinterChangedCb(lv_event_t *e) {
  handleBoxPrinterSelected(lv_dropdown_get_selected(ddMainPrinter));
}

static void boxChangedCb(lv_event_t *e) { handleBoxSelected(lv_dropdown_get_selected(ddMainBox) + 1); }

static void boxRefreshCb(lv_event_t *e) { handleBoxRefreshRequested(); }

static const lv_color_t kSlotEmptyColor = lv_color_hex(0xE53935);
static const lv_color_t kSlotLoadedColor = lv_color_hex(0x43A047);
static const lv_color_t kSlotActiveColor = lv_color_hex(0x4FD8EC);

static lv_obj_t *makeSlotLabel(lv_obj_t *parent, int x, int width, lv_label_long_mode_t mode) {
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_width(label, width);
  lv_label_set_long_mode(label, mode);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, x, 0);
  lv_label_set_text(label, "");
  return label;
}

static void buildBoxOverview() {
  // Controls row: printer, box, state, refresh
  ddMainPrinter = lv_dropdown_create(scrMain);
  lv_obj_set_size(ddMainPrinter, 200, 34);
  lv_obj_align(ddMainPrinter, LV_ALIGN_TOP_LEFT, 10, 42);
  lv_obj_add_event_cb(ddMainPrinter, boxPrinterChangedCb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_flag(ddMainPrinter, LV_OBJ_FLAG_HIDDEN);

  lblMainPrinter = lv_label_create(scrMain);
  lv_obj_set_width(lblMainPrinter, 200);
  lv_label_set_long_mode(lblMainPrinter, LV_LABEL_LONG_DOT);
  lv_obj_align(lblMainPrinter, LV_ALIGN_TOP_LEFT, 10, 50);
  lv_label_set_text(lblMainPrinter, "");

  ddMainBox = lv_dropdown_create(scrMain);
  lv_obj_set_size(ddMainBox, 84, 34);
  lv_obj_align(ddMainBox, LV_ALIGN_TOP_LEFT, 214, 42);
  lv_obj_add_event_cb(ddMainBox, boxChangedCb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_flag(ddMainBox, LV_OBJ_FLAG_HIDDEN);

  lblBoxState = lv_label_create(scrMain);
  lv_obj_set_width(lblBoxState, 120);
  lv_label_set_long_mode(lblBoxState, LV_LABEL_LONG_DOT);
  lv_obj_align(lblBoxState, LV_ALIGN_TOP_LEFT, 304, 50);
  lv_label_set_text(lblBoxState, "");

  btnBoxRefresh = lv_btn_create(scrMain);
  lv_obj_set_size(btnBoxRefresh, 42, 34);
  lv_obj_align(btnBoxRefresh, LV_ALIGN_TOP_RIGHT, -10, 42);
  lv_obj_add_event_cb(btnBoxRefresh, boxRefreshCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblRefresh = lv_label_create(btnBoxRefresh);
  lv_label_set_text(lblRefresh, LV_SYMBOL_REFRESH);
  lv_obj_center(lblRefresh);

  // Column captions
  struct Caption { StrId text; int x; };
  // The QIDI vendor and the colour swatch need no caption.
  const Caption captions[] = {{StrId::ColSlot, 14}, {StrId::ColMaterial, 58}, {StrId::ColNumber, 224},
                              {StrId::ColSpoolmanVendor, 268}};
  for (const Caption &caption : captions) {
    lv_obj_t *label = lv_label_create(scrMain);
    lv_label_set_text(label, T(caption.text));
    lv_obj_set_style_text_color(label, lv_color_hex(0x808080), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, caption.x, 79);
  }
  lv_obj_t *lblWeightCaption = lv_label_create(scrMain);
  lv_label_set_text(lblWeightCaption, T(StrId::ColWeight));
  lv_obj_set_style_text_color(lblWeightCaption, lv_color_hex(0x808080), 0);
  lv_obj_align(lblWeightCaption, LV_ALIGN_TOP_RIGHT, -16, 79);

  // The four slots, one row each
  for (int i = 0; i < 4; i++) {
    SlotRow &row = slotRows[i];
    lv_obj_t *box = lv_obj_create(scrMain);
    lv_obj_set_size(box, SCREEN_WIDTH - 20, 32);
    lv_obj_align(box, LV_ALIGN_TOP_LEFT, 10, 96 + i * 34);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x707070), 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    row.dot = lv_obj_create(box);
    lv_obj_set_size(row.dot, 12, 12);
    lv_obj_set_style_radius(row.dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(row.dot, 0, 0);
    lv_obj_set_style_bg_color(row.dot, kSlotEmptyColor, 0);
    lv_obj_align(row.dot, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_clear_flag(row.dot, LV_OBJ_FLAG_SCROLLABLE);

    row.lblSlot = makeSlotLabel(box, 24, 18, LV_LABEL_LONG_CLIP);
    row.lblMaterial = makeSlotLabel(box, 48, 78, LV_LABEL_LONG_DOT);
    row.lblQidiVendor = makeSlotLabel(box, 130, 50, LV_LABEL_LONG_DOT);

    row.swatch = lv_obj_create(box);
    lv_obj_set_size(row.swatch, 20, 20);
    lv_obj_set_style_radius(row.swatch, 3, 0);
    lv_obj_set_style_border_width(row.swatch, 1, 0);
    lv_obj_set_style_border_color(row.swatch, lv_color_hex(0x707070), 0);
    lv_obj_align(row.swatch, LV_ALIGN_LEFT_MID, 186, 0);
    lv_obj_clear_flag(row.swatch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row.swatch, LV_OBJ_FLAG_HIDDEN);

    row.lblNumber = makeSlotLabel(box, 214, 44, LV_LABEL_LONG_CLIP);
    row.lblVendor = makeSlotLabel(box, 258, 122, LV_LABEL_LONG_DOT);
    row.lblWeight = makeSlotLabel(box, 384, 68, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(row.lblWeight, LV_TEXT_ALIGN_RIGHT, 0);
  }
  uiBoxShowSlots(0, nullptr);
}

void uiBoxShowPrinters(const std::vector<String> &names, int selected, const char *info) {
  if (names.size() > 1) {
    String options;
    for (size_t i = 0; i < names.size(); i++) {
      if (i > 0) options += "\n";
      options += names[i];
    }
    lv_dropdown_set_options(ddMainPrinter, options.c_str());
    lv_dropdown_set_selected(ddMainPrinter, selected >= 0 ? selected : 0);
    lv_obj_clear_flag(ddMainPrinter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lblMainPrinter, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ddMainPrinter, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lblMainPrinter, info);
    lv_obj_clear_flag(lblMainPrinter, LV_OBJ_FLAG_HIDDEN);
  }
}

void uiBoxShowBoxes(int count, int selected) {
  // The note next to the drop-downs gets the room of the box drop-down when
  // there is none.
  lv_obj_set_x(lblBoxState, count > 1 ? 304 : 214);
  lv_obj_set_width(lblBoxState, count > 1 ? 120 : 210);
  if (count > 1) {
    String options;
    for (int i = 1; i <= count; i++) {
      if (i > 1) options += "\n";
      options += String(T(StrId::BoxLabel)) + " " + String(i);
    }
    lv_dropdown_set_options(ddMainBox, options.c_str());
    lv_dropdown_set_selected(ddMainBox, selected >= 1 ? selected - 1 : 0);
    lv_obj_clear_flag(ddMainBox, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ddMainBox, LV_OBJ_FLAG_HIDDEN);
  }
}

void uiBoxShowState(const char *text, bool error) {
  lv_label_set_text(lblBoxState, text);
  // The printer's name and address take the room of the note when there is none.
  lv_obj_set_width(lblMainPrinter, text[0] ? 200 : 290);
  lv_obj_set_style_text_color(lblBoxState, error ? lv_color_hex(0xE53935) : lv_color_hex(0x808080), 0);
}

void uiBoxShowSlots(int firstSlot, const BoxSlotView *slots) {
  for (int i = 0; i < 4; i++) {
    SlotRow &row = slotRows[i];
    const BoxSlotView *slot = slots ? &slots[i] : nullptr;

    lv_obj_set_style_bg_color(row.dot,
                              slot && slot->loaded ? (slot->active ? kSlotActiveColor : kSlotLoadedColor)
                                                   : kSlotEmptyColor,
                              0);
    // The slot within its box: 0-3, as in the app.
    lv_label_set_text(row.lblSlot, String((firstSlot + i) % 4).c_str());
    lv_label_set_text(row.lblMaterial, slot ? slot->material.c_str() : "");
    lv_label_set_text(row.lblQidiVendor, slot ? slot->qidiVendor.c_str() : "");

    const char *hex = slot ? slot->colorHex.c_str() : "";
    bool isColor = hex[0] == '#' && strlen(hex) == 7;
    if (isColor) {
      lv_obj_set_style_bg_color(row.swatch, lv_color_hex(rgbFromHex(hex)), 0);
      lv_obj_clear_flag(row.swatch, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(row.swatch, LV_OBJ_FLAG_HIDDEN);
    }

    lv_label_set_text(row.lblNumber, slot && slot->spoolNumber > 0 ? String(slot->spoolNumber).c_str() : "");
    lv_label_set_text(row.lblVendor, slot ? slot->spoolmanVendor.c_str() : "");
    lv_label_set_text(row.lblWeight, slot && slot->weightGrams >= 0 ? (String(slot->weightGrams) + " g").c_str() : "");
  }
}

static void buildMainScreen() {
  scrMain = lv_obj_create(nullptr);
  fontsApply(scrMain);

  lv_obj_t *title = lv_label_create(scrMain);
  lv_label_set_text(title, T(StrId::AppTitle));
  lv_obj_set_style_text_font(title, fontLarge, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 6);

  lv_obj_t *version = lv_label_create(scrMain);
  lv_label_set_text(version, "v" FIRMWARE_VERSION);
  lv_obj_set_style_text_font(version, fontSmall, 0);
  lv_obj_set_style_text_color(version, lv_color_hex(0x808080), 0);
  lv_obj_align_to(version, title, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -2);

  lv_obj_t *btnSettings = lv_btn_create(scrMain);
  lv_obj_set_size(btnSettings, 50, 34);
  lv_obj_align(btnSettings, LV_ALIGN_TOP_RIGHT, -10, 3);
  lv_obj_add_event_cb(btnSettings, settingsButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblGear = lv_label_create(btnSettings);
  lv_label_set_text(lblGear, LV_SYMBOL_SETTINGS);
  lv_obj_center(lblGear);

  lblStatus = lv_label_create(scrMain);
  lv_label_set_text(lblStatus, T(StrId::Ready));
  // Room for two lines at the bottom edge; a longer text ends in "...".
  lv_obj_set_size(lblStatus, SCREEN_WIDTH - 40, 34);
  lv_label_set_long_mode(lblStatus, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(lblStatus, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(lblStatus, LV_ALIGN_BOTTOM_MID, 0, -4);

  // A tag put on the reader is read automatically and opens the edit screen;
  // the button opens it as well, with empty fields, whether a tag lies on the
  // reader or not.
  lv_obj_t *btn = lv_btn_create(scrMain);
  // Between the last slot row (ends at y = 230) and the two-line status (starts at y = 282).
  lv_obj_set_size(btn, 240, 40);
  lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 235);
  lv_obj_add_event_cb(btn, editButtonCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, T(StrId::EditTag));
  lv_obj_center(label);

  buildBoxOverview();
}

static void buildEditScreen() {
  scrEdit = lv_obj_create(nullptr);
  fontsApply(scrEdit);

  // Two columns, four rows; every field has its label above it.
  //   1: Spool | Filament            (Spoolman mode only)
  //   2: Material | Colour
  //   3: Spoolman vendor (Vendor in plain mode) | Spool number
  //   4: Weight
  const int leftX = 15, rightX = 250, colW = 215;
  const int rowY[4] = {2, 58, 114, 170};
  const int controlDy = 17;

  auto makeLabel = [](int x, int y, const char *text) {
    lv_obj_t *label = lv_label_create(scrEdit);
    if (text) lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y);
    return label;
  };

  // Row 1
  lblSpoolRow = makeLabel(leftX, rowY[0], T(StrId::Spool));
  ddSpool = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddSpool, colW);
  lv_obj_align(ddSpool, LV_ALIGN_TOP_LEFT, leftX, rowY[0] + controlDy);
  lv_dropdown_set_options(ddSpool, T(StrId::NewSpool));
  lv_obj_add_event_cb(ddSpool, spoolDropdownCb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_flag(ddSpool, LV_OBJ_FLAG_HIDDEN); // holds the selection; never shown
  btnSpool = makeSwatchButton(scrEdit, colW, 38, colW - 44, &spoolSwatch, &spoolBtnLabel);
  lv_obj_align(btnSpool, LV_ALIGN_TOP_LEFT, leftX, rowY[0] + controlDy);
  lv_obj_add_event_cb(btnSpool, [](lv_event_t *e) { openPicker(true); }, LV_EVENT_CLICKED, nullptr);
  updateSpoolButton();

  lblFilament = makeLabel(rightX, rowY[0], T(StrId::Filament));
  ddFilament = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddFilament, colW);
  lv_obj_align(ddFilament, LV_ALIGN_TOP_LEFT, rightX, rowY[0] + controlDy);
  lv_dropdown_set_options(ddFilament, T(StrId::NoFilament));
  lv_obj_add_event_cb(ddFilament, filamentDropdownCb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_flag(ddFilament, LV_OBJ_FLAG_HIDDEN); // holds the selection; never shown
  btnFilament = makeSwatchButton(scrEdit, colW, 38, colW - 44, &filamentSwatch, &filamentBtnLabel);
  lv_obj_align(btnFilament, LV_ALIGN_TOP_LEFT, rightX, rowY[0] + controlDy);
  lv_obj_add_event_cb(btnFilament, [](lv_event_t *e) { openPicker(false); }, LV_EVENT_CLICKED, nullptr);
  updateFilamentButton();

  // Row 2
  makeLabel(leftX, rowY[1], T(StrId::Material));
  ddMaterial = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddMaterial, colW);
  lv_obj_align(ddMaterial, LV_ALIGN_TOP_LEFT, leftX, rowY[1] + controlDy);
  buildDropdownOptions(ddMaterial, qidiMaterials, qidiMaterialsCount);

  makeLabel(rightX, rowY[1], T(StrId::Color));
  ddColor = lv_dropdown_create(scrEdit); // holds the selection; never shown
  buildDropdownOptions(ddColor, qidiColors, qidiColorsCount);
  lv_obj_add_flag(ddColor, LV_OBJ_FLAG_HIDDEN);
  btnColor = lv_btn_create(scrEdit);
  lv_obj_set_size(btnColor, colW, 38);
  lv_obj_align(btnColor, LV_ALIGN_TOP_LEFT, rightX, rowY[1] + controlDy);
  lv_obj_add_event_cb(btnColor, openColorOverlay, LV_EVENT_CLICKED, nullptr);
  lblColorHex = lv_label_create(btnColor);
  lv_obj_center(lblColorHex);
  updateColorButton();

  // Row 3, left: Spoolman vendor (Spoolman mode) or QIDI vendor (plain mode)
  lblRow3 = makeLabel(leftX, rowY[2], nullptr);
  ddVendor = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddVendor, colW);
  lv_obj_align(ddVendor, LV_ALIGN_TOP_LEFT, leftX, rowY[2] + controlDy);
  buildDropdownOptions(ddVendor, qidiVendors, qidiVendorsCount);
  ddSpoolmanVendor = lv_dropdown_create(scrEdit);
  lv_obj_set_width(ddSpoolmanVendor, colW);
  lv_obj_align(ddSpoolmanVendor, LV_ALIGN_TOP_LEFT, leftX, rowY[2] + controlDy);

  // Row 3, right: spool number (typed in plain mode, shown read-only in
  // Spoolman mode where it comes from the selected/created spool)
  makeLabel(rightX, rowY[2], (String(T(StrId::SpoolNumber)) + " (0-999)").c_str());
  sbSpoolNumber = makeNumberField(scrEdit, rightX, rowY[2] + controlDy, 140);

  // Row 4: weight. The status line sits at the bottom edge of the screen.
  sbWeight = labeledNumberField(scrEdit, T(StrId::Weight), leftX, rowY[3]);

  lblEditStatus = lv_label_create(scrEdit);
  lv_obj_set_width(lblEditStatus, SCREEN_WIDTH - 2 * leftX);
  lv_label_set_long_mode(lblEditStatus, LV_LABEL_LONG_SCROLL_CIRCULAR); // one line; long texts scroll
  lv_obj_align(lblEditStatus, LV_ALIGN_BOTTOM_LEFT, leftX, -6);

  // Bottom button row: read another tag without going back to the main screen.
  const int btnGap = 10;
  const int btnW = (SCREEN_WIDTH - 2 * leftX - 2 * btnGap) / 3;
  struct EditButton { const char *text; lv_event_cb_t cb; };
  const EditButton editButtons[3] = {
      {T(StrId::ReadTag), readButtonCb},
      {T(StrId::WriteTag), saveButtonCb},
      {T(StrId::Back), backButtonCb},
  };
  for (int i = 0; i < 3; i++) {
    lv_obj_t *btn = lv_btn_create(scrEdit);
    lv_obj_set_size(btn, btnW, 46);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, leftX + i * (btnW + btnGap), -30);
    lv_obj_add_event_cb(btn, editButtons[i].cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, editButtons[i].text);
    lv_obj_center(label);
  }

  uiSetSpoolmanMode(false);
}

// A small flag drawn from rectangles and lines (no image data), like the app's
// language flags: Germany for "de", the United Kingdom for "en".
static lv_obj_t *flagRect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t rgb) {
  lv_obj_t *rect = lv_obj_create(parent);
  lv_obj_set_pos(rect, x, y);
  lv_obj_set_size(rect, w, h);
  lv_obj_set_style_bg_color(rect, lv_color_hex(rgb), 0);
  lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(rect, 0, 0);
  lv_obj_set_style_radius(rect, 0, 0);
  lv_obj_set_style_pad_all(rect, 0, 0);
  lv_obj_clear_flag(rect, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return rect;
}

static void flagLine(lv_obj_t *parent, const lv_point_t *points, int width, uint32_t rgb) {
  lv_obj_t *line = lv_line_create(parent);
  lv_line_set_points(line, points, 2);
  lv_obj_set_style_line_width(line, width, 0);
  lv_obj_set_style_line_color(line, lv_color_hex(rgb), 0);
  lv_obj_set_pos(line, 0, 0);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *makeFlag(lv_obj_t *parent, const char *code) {
  const int w = 24, h = 16;
  lv_obj_t *flag = lv_obj_create(parent);
  lv_obj_set_size(flag, w, h);
  lv_obj_set_style_pad_all(flag, 0, 0);
  lv_obj_set_style_radius(flag, 2, 0);
  lv_obj_set_style_border_width(flag, 0, 0);
  lv_obj_set_style_outline_width(flag, 1, 0); // keeps a light flag visible on a light background
  lv_obj_set_style_outline_color(flag, lv_color_hex(0x808080), 0);
  lv_obj_set_style_clip_corner(flag, true, 0);
  lv_obj_clear_flag(flag, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  if (strcmp(code, "de") == 0) {
    flagRect(flag, 0, 0, w, 6, 0x000000);
    flagRect(flag, 0, 5, w, 6, 0xDD0000);
    flagRect(flag, 0, 10, w, 6, 0xFFCE00);
  } else if (strcmp(code, "en") == 0) {
    static const lv_point_t diagonalDown[] = {{0, 0}, {w, h}};
    static const lv_point_t diagonalUp[] = {{w, 0}, {0, h}};
    lv_obj_set_style_bg_color(flag, lv_color_hex(0x012169), 0);
    lv_obj_set_style_bg_opa(flag, LV_OPA_COVER, 0);
    flagLine(flag, diagonalDown, 4, 0xFFFFFF);
    flagLine(flag, diagonalUp, 4, 0xFFFFFF);
    flagLine(flag, diagonalDown, 2, 0xC8102E);
    flagLine(flag, diagonalUp, 2, 0xC8102E);
    flagRect(flag, 0, 5, w, 6, 0xFFFFFF);
    flagRect(flag, 9, 0, 6, h, 0xFFFFFF);
    flagRect(flag, 0, 6, w, 4, 0xC8102E);
    flagRect(flag, 10, 0, 4, h, 0xC8102E);
  }
  return flag;
}

static const Language *currentLanguage() {
  for (size_t i = 0; i < languagesCount; i++) {
    if (settings.language == languages[i].code) return &languages[i];
  }
  return &languages[0];
}

// A flag next to a language's name, on a button or an entry of the list.
static void fillLanguageButton(lv_obj_t *btn, const Language &language) {
  lv_obj_t *flag = makeFlag(btn, language.code);
  lv_obj_align(flag, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, language.name);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 40, 0);
}

static lv_obj_t *languageOverlay;

static void closeLanguageOverlay() {
  if (!languageOverlay) return;
  lv_obj_del_async(languageOverlay);
  languageOverlay = nullptr;
}

static void languagePickedCb(lv_event_t *e) {
  const Language *language = (const Language *)lv_event_get_user_data(e);
  closeLanguageOverlay();
  // The texts are built once, so a new language takes effect with a restart.
  if (settings.language != language->code) handleLanguageSelected(language->code);
}

// The list of languages: a drop-down would show their names only, no flags.
static void openLanguageOverlay(lv_event_t *e) {
  if (languageOverlay) return;
  languageOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(languageOverlay, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_align(languageOverlay, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(languageOverlay, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(languageOverlay, LV_OPA_50, 0);
  lv_obj_set_style_border_width(languageOverlay, 0, 0);
  lv_obj_set_style_radius(languageOverlay, 0, 0);
  lv_obj_clear_flag(languageOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(
      languageOverlay,
      [](lv_event_t *ev) {
        if (lv_event_get_target(ev) == lv_event_get_current_target(ev)) closeLanguageOverlay();
      },
      LV_EVENT_CLICKED, nullptr);

  const int rowH = 46;
  lv_obj_t *panel = lv_obj_create(languageOverlay);
  lv_obj_set_size(panel, 260, languagesCount * rowH + 16);
  lv_obj_center(panel);
  lv_obj_set_style_pad_all(panel, 8, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  for (size_t i = 0; i < languagesCount; i++) {
    lv_obj_t *btn = lv_btn_create(panel);
    lv_obj_set_size(btn, 240, rowH - 6);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, i * rowH);
    lv_obj_add_event_cb(btn, languagePickedCb, LV_EVENT_CLICKED, (void *)&languages[i]);
    fillLanguageButton(btn, languages[i]);
  }
}

static lv_obj_t *makeSettingsField(lv_obj_t *parent, const char *placeholder, int y) {
  lv_obj_t *field = lv_textarea_create(parent);
  lv_textarea_set_one_line(field, true);
  lv_textarea_set_placeholder_text(field, placeholder);
  lv_obj_set_size(field, SCREEN_WIDTH - 24, 40);
  lv_obj_align(field, LV_ALIGN_TOP_LEFT, 0, y);
  lv_obj_add_event_cb(field, settingsFieldFocusCb, LV_EVENT_FOCUSED, nullptr);
  lv_obj_add_event_cb(field, settingsFieldDefocusCb, LV_EVENT_DEFOCUSED, nullptr);
  return field;
}

static void buildSettingsScreen() {
  scrSettings = lv_obj_create(nullptr);
  fontsApply(scrSettings);

  const int backH = 44;
  lv_obj_t *tabs = lv_tabview_create(scrSettings, LV_DIR_TOP, 36);
  lv_obj_set_size(tabs, SCREEN_WIDTH, SCREEN_HEIGHT - backH - 10);
  lv_obj_align(tabs, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t *tabGeneral = lv_tabview_add_tab(tabs, T(StrId::TabGeneral));
  lv_obj_t *tabWifi = lv_tabview_add_tab(tabs, T(StrId::TabWifi));
  lv_obj_t *tabPrinter = lv_tabview_add_tab(tabs, T(StrId::TabPrinter));
  lv_obj_t *tabSpoolman = lv_tabview_add_tab(tabs, T(StrId::TabSpoolman));
  for (lv_obj_t *tab : {tabGeneral, tabWifi, tabPrinter, tabSpoolman}) {
    lv_obj_set_style_pad_all(tab, 12, 0);
  }

  // General: screen sleep, firmware update
  lv_obj_t *lblSleep = lv_label_create(tabGeneral);
  lv_label_set_text(lblSleep, T(StrId::SleepTimeout));
  lv_obj_align(lblSleep, LV_ALIGN_TOP_LEFT, 0, 10);
  sbSleep = makeNumberField(tabGeneral, SCREEN_WIDTH - 24 - 120, 0, 120);
  setNumberField(sbSleep, settings.sleepMinutes);

  lv_obj_t *lblUpdateCheck = lv_label_create(tabGeneral);
  lv_label_set_text(lblUpdateCheck, T(StrId::UpdateAtStartup));
  lv_obj_align(lblUpdateCheck, LV_ALIGN_TOP_LEFT, 0, 62);
  swUpdateCheck = lv_switch_create(tabGeneral);
  lv_obj_set_size(swUpdateCheck, 50, 26);
  lv_obj_align(swUpdateCheck, LV_ALIGN_TOP_LEFT, 150, 56);
  if (settings.checkForUpdates) lv_obj_add_state(swUpdateCheck, LV_STATE_CHECKED);
  lv_obj_add_event_cb(swUpdateCheck, updateCheckToggleCb, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *btnUpdate = lv_btn_create(tabGeneral);
  lv_obj_set_size(btnUpdate, 240, 38);
  lv_obj_align(btnUpdate, LV_ALIGN_TOP_RIGHT, 0, 50);
  lv_obj_add_event_cb(btnUpdate, updateButtonCb, LV_EVENT_CLICKED, nullptr);
  lblUpdateButton = lv_label_create(btnUpdate);
  lv_label_set_text(lblUpdateButton, T(StrId::UpdateCheckNow));
  lv_obj_center(lblUpdateButton);

  lblUpdateStatus = lv_label_create(tabGeneral);
  lv_obj_set_width(lblUpdateStatus, SCREEN_WIDTH - 24);
  lv_label_set_long_mode(lblUpdateStatus, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(lblUpdateStatus, fontSmall, 0);
  lv_obj_set_style_text_color(lblUpdateStatus, lv_color_hex(0x808080), 0);
  lv_obj_align(lblUpdateStatus, LV_ALIGN_TOP_LEFT, 0, 100);
  lv_label_set_text(lblUpdateStatus, (String(T(StrId::Firmware)) + " v" + FIRMWARE_VERSION).c_str());

  // Language: the current one on a button, the list opens on a tap
  lv_obj_t *lblLanguage = lv_label_create(tabGeneral);
  lv_label_set_text(lblLanguage, T(StrId::Language));
  lv_obj_align(lblLanguage, LV_ALIGN_TOP_LEFT, 0, 142);
  lv_obj_t *btnLanguage = lv_btn_create(tabGeneral);
  lv_obj_set_size(btnLanguage, 200, 40);
  lv_obj_align(btnLanguage, LV_ALIGN_TOP_RIGHT, 0, 130);
  lv_obj_add_event_cb(btnLanguage, openLanguageOverlay, LV_EVENT_CLICKED, nullptr);
  fillLanguageButton(btnLanguage, *currentLanguage());

  // WiFi: status, reconfigure
  lblWifiStatus = lv_label_create(tabWifi);
  lv_obj_set_width(lblWifiStatus, SCREEN_WIDTH - 24);
  lv_label_set_long_mode(lblWifiStatus, LV_LABEL_LONG_WRAP);
  lv_obj_align(lblWifiStatus, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *btnReconfigure = lv_btn_create(tabWifi);
  lv_obj_set_size(btnReconfigure, 265, 50);
  lv_obj_align(btnReconfigure, LV_ALIGN_TOP_LEFT, 0, 110);
  lv_obj_add_event_cb(btnReconfigure, reconfigureNetworkCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblReconfigure = lv_label_create(btnReconfigure);
  lv_label_set_text(lblReconfigure, T(StrId::ReconfigureNetwork));
  lv_obj_center(lblReconfigure);

  // Printer: choose, add, delete, edit name and Moonraker address
  const int iconW = 52, iconGap = 8;
  ddPrinter = lv_dropdown_create(tabPrinter);
  lv_obj_set_size(ddPrinter, SCREEN_WIDTH - 24 - 2 * (iconW + iconGap), 40);
  lv_obj_align(ddPrinter, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_event_cb(ddPrinter, printerSelectedCb, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *btnPrinterNew = lv_btn_create(tabPrinter);
  lv_obj_set_size(btnPrinterNew, iconW, 40);
  lv_obj_align(btnPrinterNew, LV_ALIGN_TOP_RIGHT, -(iconW + iconGap), 0);
  lv_obj_add_event_cb(btnPrinterNew, printerNewCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblPrinterNew = lv_label_create(btnPrinterNew);
  lv_label_set_text(lblPrinterNew, LV_SYMBOL_PLUS);
  lv_obj_center(lblPrinterNew);

  btnPrinterDelete = lv_btn_create(tabPrinter);
  lv_obj_set_size(btnPrinterDelete, iconW, 40);
  lv_obj_align(btnPrinterDelete, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_event_cb(btnPrinterDelete, printerDeleteCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblPrinterDelete = lv_label_create(btnPrinterDelete);
  lv_label_set_text(lblPrinterDelete, LV_SYMBOL_TRASH);
  lv_obj_center(lblPrinterDelete);

  taPrinterName = makeSettingsField(tabPrinter, T(StrId::PrinterName), 48);
  taPrinterAddress = makeSettingsField(tabPrinter, T(StrId::PrinterAddress), 96);

  lblPrinterError = lv_label_create(tabPrinter);
  lv_obj_set_width(lblPrinterError, SCREEN_WIDTH - 24);
  lv_label_set_long_mode(lblPrinterError, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(lblPrinterError, fontSmall, 0);
  lv_obj_set_style_text_color(lblPrinterError, lv_color_hex(0xD03030), 0);
  lv_obj_align(lblPrinterError, LV_ALIGN_TOP_LEFT, 0, 142);
  lv_label_set_text(lblPrinterError, "");

  // Spoolman: address, on/off
  taSpoolmanAddress = makeSettingsField(tabSpoolman, T(StrId::SpoolmanAddress), 0);

  lv_obj_t *lblUseSpoolman = lv_label_create(tabSpoolman);
  lv_label_set_text(lblUseSpoolman, T(StrId::UseSpoolman));
  lv_obj_align(lblUseSpoolman, LV_ALIGN_TOP_LEFT, 0, 62);
  swUseSpoolman = lv_switch_create(tabSpoolman);
  lv_obj_align(swUseSpoolman, LV_ALIGN_TOP_LEFT, 185, 56);
  lv_obj_add_event_cb(swUseSpoolman, useSpoolmanToggleCb, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t *btnBack = lv_btn_create(scrSettings);
  lv_obj_set_size(btnBack, 170, backH);
  lv_obj_align(btnBack, LV_ALIGN_BOTTOM_RIGHT, -15, -5);
  lv_obj_add_event_cb(btnBack, settingsBackCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *lblBack = lv_label_create(btnBack);
  lv_label_set_text(lblBack, T(StrId::Back));
  lv_obj_center(lblBack);

  // Created last so that it is drawn above the tabs. Shared by all text fields.
  keyboard = lv_keyboard_create(scrSettings);
  lv_obj_set_size(keyboard, SCREEN_WIDTH, 140);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(keyboard, keyboardReadyCb, LV_EVENT_READY, nullptr);

  selectPrinter(settings.printers.empty() ? -1 : 0);
}

void uiInit() {
  buildMainScreen();
  buildEditScreen();
  buildSettingsScreen();
  lv_label_set_text(lblWifiStatus, T(StrId::WifiStatus));
  lv_scr_load(scrMain);
}

bool uiIsMainScreenIdle() {
  return lv_scr_act() == scrMain && lv_obj_get_child_cnt(lv_layer_top()) == 0;
}

const char *uiStatusText() { return lv_label_get_text(lblStatus); }

void uiSetStatus(const char *text) {
  lv_label_set_text(lblStatus, text);
  if (lblEditStatus) lv_label_set_text(lblEditStatus, text);
}

void uiSetWifiStatus(const char *text) {
  lv_label_set_text(lblWifiStatus, text);
}

void uiShowSpool(const FilamentSpool &spool) {
  selectCode(ddMaterial, qidiMaterials, qidiMaterialsCount, spool.materialCode);
  selectColorCode(spool.colorCode);
  selectCode(ddVendor, qidiVendors, qidiVendorsCount, spool.vendorCode);
  setNumberField(sbSpoolNumber, spool.spoolNumber);
  setNumberField(sbWeight, spool.lastWeightGrams);

  if (spoolmanModeActive) {
    // Point the Spool selector at the matching cached spool, if any, so
    // writing back does not create a duplicate; otherwise leave "New spool"
    // selected with the tag's own fields as a manual fallback.
    lv_dropdown_set_selected(ddSpool, 0);
    updateSpoolButton();
    setNewSpoolFieldsEnabled(true);
    for (size_t i = 0; i < uiSpools.size(); i++) {
      if (uiSpools[i].id == spool.spoolNumber) {
        lv_dropdown_set_selected(ddSpool, i + 1);
        spoolDropdownCb(nullptr);
        break;
      }
    }
  }

  lv_scr_load(scrEdit);
}

void uiShowEditScreenForWrite() {
  if (spoolmanModeActive) {
    setNumberField(sbSpoolNumber, 0);
    lv_dropdown_set_selected(ddSpool, 0);
    updateSpoolButton();
    setNewSpoolFieldsEnabled(true);
  }
  lv_scr_load(scrEdit);
}

void uiSetSpoolmanMode(bool active) {
  spoolmanModeActive = active;
  lv_label_set_text(lblRow3, active ? T(StrId::SpoolmanVendor) : T(StrId::Vendor));

  auto show = [](lv_obj_t *obj, bool visible) {
    if (visible) {
      lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
  };
  show(ddVendor, !active);
  show(ddSpoolmanVendor, active);
  show(lblSpoolRow, active);
  show(btnSpool, active);
  show(lblFilament, active);
  show(btnFilament, active);

  // In Spoolman mode the number comes from the selected/created spool.
  if (active) {
    lv_obj_add_state(sbSpoolNumber, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(sbSpoolNumber, LV_STATE_DISABLED);
  }
}

void uiSetSpoolmanLists(const std::vector<SpoolmanVendor> &vendors,
                        const std::vector<SpoolmanSpool> &spools,
                        const std::vector<SpoolmanFilament> &filaments) {
  uiVendors = vendors;
  uiSpools = spools;
  uiFilaments = filaments;
  // Newest spool first (highest Spoolman id on top), same as the Flutter
  // app's spool dropdown. All lookups below match by id, so this sort
  // doesn't affect anything but display order.
  std::sort(uiSpools.begin(), uiSpools.end(),
            [](const SpoolmanSpool &a, const SpoolmanSpool &b) { return a.id > b.id; });
  // Vendor, then material, then colour - same order as the Flutter app's
  // filament dropdown.
  std::sort(uiFilaments.begin(), uiFilaments.end(),
            [](const SpoolmanFilament &a, const SpoolmanFilament &b) {
              if (a.vendorName != b.vendorName) return a.vendorName < b.vendorName;
              if (a.material != b.material) return a.material < b.material;
              return a.colorHex < b.colorHex;
            });

  String vendorOptions;
  for (size_t i = 0; i < uiVendors.size(); i++) {
    if (i > 0) vendorOptions += "\n";
    vendorOptions += uiVendors[i].name;
  }
  if (vendorOptions.length() == 0) vendorOptions = "-";
  lv_dropdown_set_options(ddSpoolmanVendor, vendorOptions.c_str());

  String spoolOptions = T(StrId::NewSpool);
  for (const SpoolmanSpool &spool : uiSpools) {
    spoolOptions += "\n#" + String(spool.id) + " " + spool.material;
    if (spool.vendorName.length()) spoolOptions += " (" + spool.vendorName + ")";
    spoolOptions += " - " + String(spool.remainingWeightGrams) + "g";
  }
  lv_dropdown_set_options(ddSpool, spoolOptions.c_str());
  lv_dropdown_set_selected(ddSpool, 0);

  String filamentOptions = T(StrId::NoFilament);
  for (const SpoolmanFilament &filament : uiFilaments) {
    filamentOptions += "\n" + filament.vendorName + " " + filament.material;
    if (filament.colorHex.length()) filamentOptions += " " + filament.colorHex;
  }
  lv_dropdown_set_options(ddFilament, filamentOptions.c_str());
  lv_dropdown_set_selected(ddFilament, 0);
  updateSpoolButton();
  updateFilamentButton();
}

bool uiIsNewSpoolSelected() {
  return !spoolmanModeActive || lv_dropdown_get_selected(ddSpool) == 0;
}

int uiSelectedExistingSpoolId() {
  if (uiIsNewSpoolSelected()) return -1;
  uint16_t index = lv_dropdown_get_selected(ddSpool);
  if (index - 1 >= uiSpools.size()) return -1;
  return uiSpools[index - 1].id;
}

int uiSelectedFilamentId() {
  if (!spoolmanModeActive || !uiIsNewSpoolSelected()) return -1;
  uint16_t index = lv_dropdown_get_selected(ddFilament);
  if (index == 0 || index - 1 >= uiFilaments.size()) return -1;
  return uiFilaments[index - 1].id;
}

int uiSelectedSpoolmanVendorId() {
  if (!spoolmanModeActive) return -1;
  if (!uiIsNewSpoolSelected()) {
    // An existing spool brings its own vendor, whatever the dropdown shows.
    uint16_t spoolIndex = lv_dropdown_get_selected(ddSpool);
    if (spoolIndex - 1 < uiSpools.size() && uiSpools[spoolIndex - 1].vendorId > 0) {
      return uiSpools[spoolIndex - 1].vendorId;
    }
    return -1;
  }
  if (uiVendors.empty()) return -1;
  uint16_t index = lv_dropdown_get_selected(ddSpoolmanVendor);
  if (index >= uiVendors.size()) return -1;
  return uiVendors[index].id;
}

FilamentSpool uiCurrentSpool() {
  FilamentSpool spool;
  spool.materialCode = selectedCode(ddMaterial, qidiMaterials, qidiMaterialsCount);
  spool.colorCode = selectedCode(ddColor, qidiColors, qidiColorsCount);
  spool.lastWeightGrams = (uint16_t)getNumberField(sbWeight);

  if (spoolmanModeActive) {
    // spoolNumber/internalVendorId/vendorCode are resolved by main.cpp
    // (existing spool id, or a newly created one) after this call.
    // Placeholder: main.cpp decides the QIDI vendor code (no dropdown here).
    spool.vendorCode = qidiVendorCode("QIDI");
    spool.internalVendorId = 0;
    spool.spoolNumber = 0;
  } else {
    spool.vendorCode = selectedCode(ddVendor, qidiVendors, qidiVendorsCount);
    spool.internalVendorId = 0;
    spool.spoolNumber = (uint16_t)getNumberField(sbSpoolNumber);
  }
  return spool;
}
