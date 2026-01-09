#pragma once
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t app_lcd_init(void);
    esp_err_t app_lvgl_init(void);
    // 初始化 I2C 总线、复位 CST816D、做一次探测
    esp_err_t ctp_init(void);

    // 将触摸注册为 LVGL 输入设备；返回 indev 句柄，可忽略返回值
    lv_indev_t *ctp_register_lvgl(lv_display_t *disp);

#ifdef __cplusplus
}
#endif
