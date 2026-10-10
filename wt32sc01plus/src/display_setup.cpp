#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include "fonts.h"
#include <driver/i2c.h>
#include "board_config.h"
#include "display_setup.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 _panel_instance;
  lgfx::Bus_Parallel8 _bus_instance;
  lgfx::Touch_FT5x06 _touch_instance; // FT6336 shares the FT5x06 protocol

public:
  LGFX() {
    {
      auto cfg = _bus_instance.config();
      cfg.freq_write = 20000000;
      cfg.pin_wr = TFT_WR;
      cfg.pin_rd = TFT_RD;
      cfg.pin_rs = TFT_RS;
      cfg.pin_d0 = TFT_D0;
      cfg.pin_d1 = TFT_D1;
      cfg.pin_d2 = TFT_D2;
      cfg.pin_d3 = TFT_D3;
      cfg.pin_d4 = TFT_D4;
      cfg.pin_d5 = TFT_D5;
      cfg.pin_d6 = TFT_D6;
      cfg.pin_d7 = TFT_D7;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = -1;
      cfg.pin_rst = TFT_RST;
      cfg.pin_busy = -1;
      cfg.panel_width = PANEL_WIDTH;
      cfg.panel_height = PANEL_HEIGHT;
      cfg.readable = false;
      cfg.invert = true;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 0;
      cfg.x_max = PANEL_WIDTH - 1;
      cfg.y_min = 0;
      cfg.y_max = PANEL_HEIGHT - 1;
      cfg.pin_int = TOUCH_INT;
      cfg.pin_rst = TOUCH_RST;
      cfg.pin_sda = TOUCH_SDA;
      cfg.pin_scl = TOUCH_SCL;
      cfg.i2c_addr = 0x38;
      cfg.i2c_port = I2C_NUM_0;
      cfg.freq = 400000;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

static LGFX lcd;
static lv_disp_draw_buf_t drawBuf;
static lv_color_t buf1[SCREEN_WIDTH * 40];
static lv_disp_drv_t dispDrv;
static lv_indev_drv_t indevDrv;

static void lvglFlush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *colorP) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  lcd.startWrite();
  lcd.setAddrWindow(area->x1, area->y1, w, h);
  lcd.writePixels((lgfx::rgb565_t *)colorP, w * h);
  lcd.endWrite();
  lv_disp_flush_ready(drv);
}

static uint32_t sleepTimeoutMs = 0;
static uint32_t lastActivityMs = 0;
static bool sleeping = false;
static bool swallowTouch = false; // the touch that woke the screen, until released

static void wakeScreen() {
  sleeping = false;
  lastActivityMs = millis();
  digitalWrite(TFT_BL, HIGH);
}

void displayWake() {
  if (sleeping) {
    wakeScreen();
  } else {
    lastActivityMs = millis();
  }
}

void displaySetSleepTimeout(uint8_t minutes) {
  sleepTimeoutMs = (uint32_t)minutes * 60000UL;
  lastActivityMs = millis();
  if (sleeping && sleepTimeoutMs == 0) wakeScreen();
}

static void lvglTouchRead(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  lgfx::touch_point_t tp;
  bool touched = lcd.getTouch(&tp);
  if (touched) lastActivityMs = millis();

  if (sleeping) {
    if (touched) {
      wakeScreen();
      swallowTouch = true;
    }
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  if (swallowTouch) {
    if (touched) {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }
    swallowTouch = false;
  }

  if (touched) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = tp.x;
    data->point.y = tp.y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void displaySetup() {
  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(200);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  lastActivityMs = millis();

  lv_init();

  lv_disp_draw_buf_init(&drawBuf, buf1, nullptr, SCREEN_WIDTH * 40);

  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = SCREEN_WIDTH;
  dispDrv.ver_res = SCREEN_HEIGHT;
  dispDrv.flush_cb = lvglFlush;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);
  fontsInit();

  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = lvglTouchRead;
  lv_indev_drv_register(&indevDrv);
}

void displayLoop() {
  if (!sleeping && sleepTimeoutMs > 0 && millis() - lastActivityMs > sleepTimeoutMs) {
    sleeping = true;
    digitalWrite(TFT_BL, LOW);
  }
  lv_timer_handler();
}
