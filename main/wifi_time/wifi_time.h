#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
// 校准状态
typedef enum {
    UI_TIME_UPDATE,     // 更新时间字符串
    UI_TIME_SYNC_OK,    // 校准成功
    UI_TIME_SYNC_FAIL,  // 校准失败
    UI_TIME_SYNCING,

} ui_time_event_t;

// 校准字符串
typedef struct {
    ui_time_event_t type;
    char time_str[32];   // "YYYY-MM-DD HH:MM:SS"
    char updating_msg[64];
} ui_time_msg_t;


bool ui_time_dequeue(ui_time_msg_t *out) ; 


// 系统初始化（开机调用一次）
void wifi_time_sys_init(void);

// 设置 WiFi AP（写入 NVS）
void wifi_time_set_ap(const char *ssid, const char *password);

// 立即进行一次校时（按钮触发）
void wifi_time_sync_now(void);


void ui_time_timer_start(void); 


 void wifi_time_task_init(void) ; 

void ui_time_queue_init() ; 

extern  SemaphoreHandle_t s_time_sync_sem ; 

#ifdef __cplusplus
}
#endif
