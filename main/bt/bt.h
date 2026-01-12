#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ===== 对外接口 ===== */
void bt_init(void);
void bt_start_scan(void);
void bt_task(); 

/* ===== 给 LVGL 用的 ===== */
extern TaskHandle_t bt_task_handle;

#ifdef __cplusplus
}
#endif
