
#include "vars.h"
#include <stdint.h>
#include <stdio.h>
#include "lcd/lcd.h"
#include "esp_err.h"
#include "ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "time.h"
#include "esp_log.h"
#include "ns4168/audio.h"
#include "sd/sd.h"
#include "screens.h"
#include "esp_lvgl_port.h"

static const char *TAG = "main";

static const char *s_playlist[MAX_FILES + 1];
static char s_playlist_paths[MAX_FILES][128];

static int build_playlist_from_sd(void)
{
  int num_files = scan_files_by_extension(".wav");
  if (num_files <= 0) {
    return 0;
  }

  char (*files)[MAX_FILENAME] = get_filtered_files();
  if (!files) {
    return 0;
  }

  for (int i = 0; i < num_files; i++) {
    snprintf(s_playlist_paths[i], sizeof(s_playlist_paths[i]), "/sdcard/%s", files[i]);
    s_playlist[i] = s_playlist_paths[i];
  }
  s_playlist[num_files] = NULL;
  return num_files;
}


static void lvgl_ui_timer_cb(lv_timer_t *timer)
{
  (void)timer;
  wav_player_ui_msg_t msg;
  while (wav_player_ui_dequeue(&msg)) {
    if (msg.type == WAV_UI_UPDATE_TIME) {
      set_var_current_song_time(msg.current_time);
      set_var_song_fulltime(msg.total_time);
      lv_bar_set_value(objects.music_process_bar, msg.percent, LV_ANIM_OFF);
      // ESP_LOGI("PER","当前百分比是%d",msg.percent) ; 
    } else if (msg.type == WAV_UI_UPDATE_LRC) {
      set_var_current_lrc(msg.lrc);
    } else if (msg.type == WAV_UI_CLEAR_LRC) {
      set_var_current_lrc("");
    }
  }

  ui_tick();
}



/* 主函数入口 */
void app_main(void)
{
  sdcard_init();

  ESP_ERROR_CHECK(app_lcd_init());
  ESP_ERROR_CHECK(app_lvgl_init());
  ctp_init();
  ui_init();
  if (lvgl_port_lock(0)) {
    lv_timer_create(lvgl_ui_timer_cb, 200, NULL);
    lvgl_port_unlock();
  }

  // 2. 初始化播放器
  int playlist_count = build_playlist_from_sd();
  if (playlist_count <= 0)
  {
    ESP_LOGW(TAG, "No .wav files found on SD card");
    return;
  }

  esp_err_t ret = wav_player_init(s_playlist, 0.05f); // 音量 50%
  if (ret != ESP_OK)
  {
    ESP_LOGE("main", "Failed to init wav player");
    return;
  }

    // int num_files = scan_files_by_extension(".wav");
    // if (num_files > 0) {
    //     char (*files)[50] = get_filtered_files();
    //     ESP_LOGI(TAG, "Listing .jpg files:");
    //     for (int i = 0; i < num_files; i++) {
    //         printf("  [%d] %s\n", i, files[i]);
    //     }
    // } else {
    //     ESP_LOGW(TAG, "No .wav files found.");
    // }


}


// 可以使用这种命令来进行播放的切换等等

//    wav_player_send_cmd(WAV_CMD_STOP);
