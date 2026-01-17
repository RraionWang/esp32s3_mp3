#pragma once

#include <stdbool.h>
#include "esp_err.h"

/* 录音命令 */
typedef enum {
    REC_CMD_START,
    REC_CMD_STOP,
} recorder_cmd_t;

/* 初始化（I2S + 任务 + 队列） */
esp_err_t recorder_init(void);

/* 开始录音（自动分配 REC0000~REC9999） */
esp_err_t recorder_start(void);

/* 停止录音 */
esp_err_t recorder_stop(void);

/* 当前是否正在录音 */
bool recorder_is_recording(void);

/* 当前录音文件路径（录音中才有效） */
const char *recorder_current_path(void);
