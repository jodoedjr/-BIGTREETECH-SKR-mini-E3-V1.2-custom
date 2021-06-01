/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/**
 * @file tft_lvgl_configuration.cpp
 * @date    2020-02-21
 */

#include "../../../../inc/MarlinConfigPre.h"

#if HAS_TFT_LVGL_UI

#include "SPI_TFT.h"

#include "tft_lvgl_configuration.h"
#include "draw_ready_print.h"

#include "pic_manager.h"
#include "mks_hardware_test.h"
#include "draw_ui.h"
#include "SPIFlashStorage.h"
#include <lvgl.h>

#include "../../../../MarlinCore.h"
#include "../../../../inc/MarlinConfig.h"

#include HAL_PATH(../../HAL, tft/xpt2046.h)
#include "../../../ultralcd.h"
XPT2046 touch;

#if ENABLED(POWER_LOSS_RECOVERY)
  #include "../../../../feature/powerloss.h"
#endif

#include <SPI.h>

#ifndef TFT_WIDTH
  #define TFT_WIDTH  480
#endif
#ifndef TFT_HEIGHT
  #define TFT_HEIGHT 320
#endif

#if HAS_SPI_FLASH_FONT
  extern void init_gb2312_font();
#endif

static lv_disp_buf_t disp_buf;
lv_group_t*  g;
#if ENABLED(SDSUPPORT)
  extern void UpdateAssets();
#endif
uint16_t DeviceCode = 0x9488;
extern uint8_t sel_id;

extern uint8_t gcode_preview_over, flash_preview_begin, default_preview_flg;

uint8_t bmp_public_buf[17 * 1024];

void SysTick_Callback() {
  lv_tick_inc(1);
  print_time_count();
  #if ENABLED(USE_WIFI_FUNCTION)
    if (tips_disp.timer == TIPS_TIMER_START) {
      tips_disp.timer_count++;
    }
  #endif
  if (uiCfg.filament_loading_time_flg == 1) {
    uiCfg.filament_loading_time_cnt++;
    uiCfg.filament_rate = (uint32_t)(((uiCfg.filament_loading_time_cnt / (uiCfg.filament_loading_time * 1000.0)) * 100.0) + 0.5);
    if (uiCfg.filament_loading_time_cnt >= (uiCfg.filament_loading_time * 1000)) {
      uiCfg.filament_loading_time_cnt  = 0;
      uiCfg.filament_loading_time_flg  = 0;
      uiCfg.filament_loading_completed = 1;
    }
  }
  if (uiCfg.filament_unloading_time_flg == 1) {
    uiCfg.filament_unloading_time_cnt++;
    uiCfg.filament_rate = (uint32_t)(((uiCfg.filament_unloading_time_cnt / (uiCfg.filament_unloading_time * 1000.0)) * 100.0) + 0.5);
    if (uiCfg.filament_unloading_time_cnt >= (uiCfg.filament_unloading_time * 1000)) {
      uiCfg.filament_unloading_time_cnt  = 0;
      uiCfg.filament_unloading_time_flg  = 0;
      uiCfg.filament_unloading_completed = 1;
      uiCfg.filament_rate                = 100;
    }
  }
}

extern uint8_t bmp_public_buf[17 * 1024];

void tft_lvgl_init() {

  //uint16_t test_id=0;
  W25QXX.init(SPI_QUARTER_SPEED);
  //test_id=W25QXX.W25QXX_ReadID();

  gCfgItems_init();
  ui_cfg_init();
  disp_language_init();

  //init tft first!
  SPI_TFT.spi_init(SPI_FULL_SPEED);
  SPI_TFT.LCD_init();

  //spi_flash_read_test();
  #if ENABLED(SDSUPPORT)
    watchdog_refresh();
    UpdateAssets();
  #endif

  watchdog_refresh();
  mks_test_get();

  touch.Init();

  lv_init();

  lv_disp_buf_init(&disp_buf, bmp_public_buf, NULL, LV_HOR_RES_MAX * 18); /*Initialize the display buffer*/

  lv_disp_drv_t disp_drv;     /*Descriptor of a display driver*/
  lv_disp_drv_init(&disp_drv);    /*Basic initialization*/
  disp_drv.flush_cb = my_disp_flush; /*Set your driver function*/
  disp_drv.buffer = &disp_buf;    /*Assign the buffer to the display*/
  lv_disp_drv_register(&disp_drv);  /*Finally register the driver*/

  lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);     /*Descriptor of a input device driver*/
  indev_drv.type = LV_INDEV_TYPE_POINTER; /*Touch pad is a pointer-like device*/
  indev_drv.read_cb = my_touchpad_read;  /*Set your driver function*/
  lv_indev_drv_register(&indev_drv);   /*Finally register the driver*/

  #if HAS_ROTARY_ENCODER
    g = lv_group_create();
    lv_indev_drv_t enc_drv;
    lv_indev_drv_init(&enc_drv);
    enc_drv.type = LV_INDEV_TYPE_ENCODER;
    enc_drv.read_cb = my_mousewheel_read;
    lv_indev_t * enc_indev = lv_indev_drv_register(&enc_drv);
    lv_indev_set_group(enc_indev, g);
  #endif

  lv_fs_drv_t spi_flash_drv;
  lv_fs_drv_init(&spi_flash_drv);
  spi_flash_drv.letter = 'F';
  spi_flash_drv.open_cb = spi_flash_open_cb;
  spi_flash_drv.close_cb = spi_flash_close_cb;
  spi_flash_drv.read_cb = spi_flash_read_cb;
  spi_flash_drv.seek_cb = spi_flash_seek_cb;
  spi_flash_drv.tell_cb = spi_flash_tell_cb;
  lv_fs_drv_register(&spi_flash_drv);

  lv_fs_drv_t sd_drv;
  lv_fs_drv_init(&sd_drv);
  sd_drv.letter = 'S';
  sd_drv.open_cb = sd_open_cb;
  sd_drv.close_cb = sd_close_cb;
  sd_drv.read_cb = sd_read_cb;
  sd_drv.seek_cb = sd_seek_cb;
  sd_drv.tell_cb = sd_tell_cb;
  lv_fs_drv_register(&sd_drv);

  systick_attach_callback(SysTick_Callback);

  #if HAS_SPI_FLASH_FONT
    init_gb2312_font();
  #endif

  tft_style_init();

  filament_pin_setup();

  lv_encoder_pin_init();

  #if ENABLED(POWER_LOSS_RECOVERY)
    recovery.load();
    if (recovery.valid()) {
      if (gCfgItems.from_flash_pic == 1)
        flash_preview_begin = 1;
      else
        default_preview_flg = 1;

      uiCfg.print_state = REPRINTING;

      ZERO(public_buf_m);
      strncpy(public_buf_m, recovery.info.sd_filename, sizeof(public_buf_m));
      card.printLongPath(public_buf_m);

      strncpy(list_file.long_name[sel_id], card.longFilename, sizeof(list_file.long_name[sel_id]));

      lv_draw_printing();
    }
    else
  #endif
    lv_draw_ready_print();

  if (mks_test_flag == 0x1E)
    mks_gpio_test();
}

void my_disp_flush(lv_disp_drv_t * disp, const lv_area_t * area, lv_color_t * color_p) {
  uint16_t i, width, height;

  width = area->x2 - area->x1 + 1;
  height = area->y2 - area->y1 + 1;

  SPI_TFT.setWindow((uint16_t)area->x1, (uint16_t)area->y1, width, height);
  for (i = 0; i < height; i++) {
    SPI_TFT.tftio.WriteSequence((uint16_t*)(color_p + width * i), width);
  }
  lv_disp_flush_ready(disp);       /* Indicate you are ready with the flushing*/

  W25QXX.init(SPI_QUARTER_SPEED);
}

#define TICK_CYCLE 1

unsigned int getTickDiff(unsigned int curTick, unsigned int lastTick) {
  return TICK_CYCLE * (lastTick <= curTick ? (curTick - lastTick) : (0xFFFFFFFF - lastTick + curTick));
}

static bool get_point(int16_t *x, int16_t *y) {
  bool is_touched = touch.getRawPoint(x, y);

  if (is_touched) {
    *x = int16_t((int32_t(*x) * XPT2046_X_CALIBRATION) >> 16) + XPT2046_X_OFFSET;
    *y = int16_t((int32_t(*y) * XPT2046_Y_CALIBRATION) >> 16) + XPT2046_Y_OFFSET;
  }

  #if (TFT_ROTATION & TFT_ROTATE_180)
    *x = int16_t((TFT_WIDTH) - (int)(*x));
    *y = int16_t((TFT_HEIGHT) - (int)(*y));
  #endif

  return is_touched;
}

bool my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
  static int16_t last_x = 0, last_y = 0;
  static uint8_t last_touch_state = LV_INDEV_STATE_REL;
  static int32_t touch_time1 = 0;
  uint32_t tmpTime, diffTime = 0;

  tmpTime = millis();
  diffTime = getTickDiff(tmpTime, touch_time1);
  /*Save the state and save the pressed coordinate*/
  //data->state = TOUCH_PressValid(last_x, last_y) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
  //if (data->state == LV_INDEV_STATE_PR)  ADS7843_Rd_Addata((u16 *)&last_x, (u16 *)&last_y);
  //touchpad_get_xy(&last_x, &last_y);
  /*Save the pressed coordinates and the state*/
  if (diffTime > 20) {
    if (get_point(&last_x, &last_y)) {

      if (last_touch_state == LV_INDEV_STATE_PR) return false;
      data->state = LV_INDEV_STATE_PR;

      // Set the coordinates (if released use the last-pressed coordinates)

      data->point.x = last_x;
      data->point.y = last_y;

      last_x = last_y = 0;
      last_touch_state = LV_INDEV_STATE_PR;
    }
    else {
      if (last_touch_state == LV_INDEV_STATE_PR)
        data->state = LV_INDEV_STATE_REL;
      last_touch_state = LV_INDEV_STATE_REL;
    }

    touch_time1 = tmpTime;
  }

  return false; // Return `false` since no data is buffering or left to read
}

int16_t enc_diff = 0;
lv_indev_state_t state = LV_INDEV_STATE_REL;

bool my_mousewheel_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
  (void) indev_drv;   /*Unused*/

  data->state = state;
  data->enc_diff = enc_diff;
  enc_diff = 0;

  return false;       /*No more data to read so return false*/
}

extern uint8_t currentFlashPage;

//spi_flash
uint32_t pic_read_base_addr = 0, pic_read_addr_offset = 0;
lv_fs_res_t spi_flash_open_cb (lv_fs_drv_t * drv, void * file_p, const char * path, lv_fs_mode_t mode) {
  static char last_path_name[30];
  if (strcasecmp(last_path_name,path) != 0) {
    pic_read_base_addr = lv_get_pic_addr((uint8_t *)path);
    ZERO(last_path_name);
    strcpy(last_path_name,path);
  }
  else {
    W25QXX.init(SPI_QUARTER_SPEED);
    currentFlashPage = 0;
  }
  pic_read_addr_offset = pic_read_base_addr;
  return LV_FS_RES_OK;
}

lv_fs_res_t spi_flash_close_cb (lv_fs_drv_t * drv, void * file_p) {
  lv_fs_res_t res = LV_FS_RES_OK;
  /* Add your code here*/
  pic_read_addr_offset = pic_read_base_addr;
  return res;
}

lv_fs_res_t spi_flash_read_cb (lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
  lv_pic_test((uint8_t *)buf, pic_read_addr_offset, btr);
  *br = btr;
  return LV_FS_RES_OK;
}

lv_fs_res_t spi_flash_seek_cb(lv_fs_drv_t * drv, void * file_p, uint32_t pos) {
  #if HAS_SPI_FLASH_COMPRESSION
    if (pos == 4) {
      uint8_t bmp_header[4];
      SPIFlash.beginRead(pic_read_base_addr);
      SPIFlash.readData(bmp_header, 4);
      currentFlashPage = 1;
    }
    pic_read_addr_offset = pic_read_base_addr;
  #else
    pic_read_addr_offset = pic_read_base_addr + pos;
  #endif
  return LV_FS_RES_OK;
}

lv_fs_res_t spi_flash_tell_cb(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
  *pos_p = pic_read_addr_offset - pic_read_base_addr;
  return LV_FS_RES_OK;
}

//sd
char *cur_namefff;
uint32_t sd_read_base_addr = 0,sd_read_addr_offset = 0;
lv_fs_res_t sd_open_cb (lv_fs_drv_t * drv, void * file_p, const char * path, lv_fs_mode_t mode) {
  //cur_namefff = strrchr(path, '/');
  char name_buf[100];
  ZERO(name_buf);
  strcat(name_buf,"/");
  strcat(name_buf,path);
  char *temp = strstr(name_buf,".bin");
  if (temp) { strcpy(temp,".GCO"); }
  sd_read_base_addr = lv_open_gcode_file((char *)name_buf);
  sd_read_addr_offset = sd_read_base_addr;
  if (sd_read_addr_offset == 0) return LV_FS_RES_NOT_EX;
  return LV_FS_RES_OK;
}

lv_fs_res_t sd_close_cb (lv_fs_drv_t * drv, void * file_p) {
  /* Add your code here*/
  lv_close_gcode_file();
  return LV_FS_RES_OK;
}

lv_fs_res_t sd_read_cb (lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
  if (btr == 200) {
    lv_gcode_file_read((uint8_t *)buf);
    //pic_read_addr_offset += 208;
    *br = 200;
  }
  else if (btr == 4) {
    uint8_t header_pic[4] = { 0x04, 0x90, 0x81, 0x0C };
    memcpy(buf, header_pic, 4);
    //pic_read_addr_offset += 4;
    *br = 4;
  }
  return LV_FS_RES_OK;
}

lv_fs_res_t sd_seek_cb(lv_fs_drv_t * drv, void * file_p, uint32_t pos) {
  sd_read_addr_offset = sd_read_base_addr + (pos - 4) / 200 * 409;
  lv_gcode_file_seek(sd_read_addr_offset);
  return LV_FS_RES_OK;
}

lv_fs_res_t sd_tell_cb(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
  if (sd_read_addr_offset) *pos_p = 0;
  else *pos_p = (sd_read_addr_offset - sd_read_base_addr) / 409 * 200 + 4;
  return LV_FS_RES_OK;
}

void lv_encoder_pin_init() {
  #if 1 // HAS_DIGITAL_BUTTONS

    #if BUTTON_EXISTS(EN1)
      SET_INPUT_PULLUP(BTN_EN1);
    #endif
    #if BUTTON_EXISTS(EN2)
      SET_INPUT_PULLUP(BTN_EN2);
    #endif
    #if BUTTON_EXISTS(ENC)
      SET_INPUT_PULLUP(BTN_ENC);
    #endif

    #if BUTTON_EXISTS(BACK)
      SET_INPUT_PULLUP(BTN_BACK);
    #endif

    #if BUTTON_EXISTS(UP)
      SET_INPUT(BTN_UP);
    #endif
    #if BUTTON_EXISTS(DWN)
      SET_INPUT(BTN_DWN);
    #endif
    #if BUTTON_EXISTS(LFT)
      SET_INPUT(BTN_LFT);
    #endif
    #if BUTTON_EXISTS(RT)
      SET_INPUT(BTN_RT);
    #endif

  #endif // HAS_DIGITAL_BUTTONS
}

#if 1 // HAS_ENCODER_ACTION

  //static const int8_t encoderDirection = 1;
  //static int16_t enc_Direction;
  void lv_update_encoder() {
    static uint32_t encoder_time1;
    uint32_t tmpTime, diffTime = 0;
    tmpTime = millis();
    diffTime = getTickDiff(tmpTime, encoder_time1);
    if (diffTime > 50) {

      #if HAS_ENCODER_WHEEL

        #if ANY_BUTTON(EN1, EN2, ENC, BACK)

          uint8_t newbutton = 0;

          #if BUTTON_EXISTS(EN1)
            if (BUTTON_PRESSED(EN1)) newbutton |= EN_A;
          #endif
          #if BUTTON_EXISTS(EN2)
            if (BUTTON_PRESSED(EN2)) newbutton |= EN_B;
          #endif
          #if BUTTON_EXISTS(ENC)
            if (BUTTON_PRESSED(ENC)) newbutton |= EN_C;
          #endif
          #if BUTTON_EXISTS(BACK)
            if (BUTTON_PRESSED(BACK)) newbutton |= EN_D;
          #endif

        #else

          constexpr uint8_t newbutton = 0;

        #endif


        static uint8_t buttons = 0;
        buttons = newbutton;
        static uint8_t lastEncoderBits;

        #define encrot0 0
        #define encrot1 1
        #define encrot2 2

        // Manage encoder rotation
        //#define ENCODER_SPIN(_E1, _E2) switch (lastEncoderBits) { case _E1: enc_Direction += encoderDirection; break; case _E2: enc_Direction -= encoderDirection; }

        uint8_t enc = 0;
        if (buttons & EN_A) enc |= B01;
        if (buttons & EN_B) enc |= B10;
        if (enc != lastEncoderBits) {
          switch (enc) {
            case encrot1:
              if (lastEncoderBits == encrot0) {
                enc_diff--;
                encoder_time1 = tmpTime;
              }
              break;
            case encrot2:
              if (lastEncoderBits == encrot0) {
                enc_diff++;
                encoder_time1 = tmpTime;
              }
              break;
          }
          lastEncoderBits = enc;
        }
        static uint8_t last_button_state = LV_INDEV_STATE_REL;
        const uint8_t enc_c = (buttons & EN_C) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
        if (enc_c != last_button_state) {
          state = enc_c ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;

          last_button_state = enc_c;
        }

      #endif // HAS_ENCODER_WHEEL

    } // next_button_update_ms
  }

#endif // HAS_ENCODER_ACTION

#endif // HAS_TFT_LVGL_UI
