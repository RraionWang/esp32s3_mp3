// wav_player.h
#ifndef __WAV_PLAYER_H__
#define __WAV_PLAYER_H__

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 播放器命令类型
 */
typedef enum {
    WAV_CMD_PLAY,   // 保留（可自动播放）
    WAV_CMD_PAUSE,
    WAV_CMD_RESUME,
    WAV_CMD_STOP,
    WAV_CMD_NEXT,
    WAV_CMD_PREV,
    WAV_CMD_PLAY_INDEX,
    WAV_CMD_SET_VOLUME,   // ← 新增这一行
} wav_player_cmd_t;

typedef enum {
    WAV_UI_UPDATE_TIME,
    WAV_UI_UPDATE_LRC,
    WAV_UI_CLEAR_LRC
} wav_player_ui_msg_type_t;

typedef struct {
    wav_player_ui_msg_type_t type;
    char current_time[8];
    char total_time[8];
    uint8_t percent;
    char lrc[64];
} wav_player_ui_msg_t;

bool wav_player_ui_dequeue(wav_player_ui_msg_t *out);


esp_err_t wav_player_set_volume_cmd(float volume);
esp_err_t wav_player_play_index(int index);

/**
 * @brief 初始化 WAV 播放器
 *
 * @param playlist 歌单（字符串数组，必须以 NULL 结尾）
 * @param volume 初始音量 (0.0 ～ 1.0)
 * @return esp_err_t
 */
esp_err_t wav_player_init(const char *playlist[], float volume);

/**
 * @brief 发送控制命令
 *
 * @param cmd 命令
 * @return esp_err_t
 */
esp_err_t wav_player_send_cmd(wav_player_cmd_t cmd);

/**
 * @brief 设置音量
 *
 * @param volume 0.0 ～ 1.0
 */
void wav_player_set_volume(float volume);

#ifdef __cplusplus
}
#endif

#endif // __WAV_PLAYER_H__
