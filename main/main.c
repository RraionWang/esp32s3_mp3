
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
#include "mytimer.h"
#include "record/record.h"
#include "i2c_sensor/aht20.h"
#include "i2c_sensor/qmi.h"
#include "draw_cube.h"
#include "wifi_time.h"

static const char *TAG = "main";

static const char *s_playlist[MAX_FILES + 1];
static char s_playlist_paths[MAX_FILES][128];

static int build_playlist_from_sd(void)
{
  int num_files = scan_files_by_extension(".wav");
  if (num_files <= 0)
  {
    return 0;
  }

  char (*files)[MAX_FILENAME] = get_filtered_files();
  if (!files)
  {
    return 0;
  }

  for (int i = 0; i < num_files; i++)
  {
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
  ui_time_msg_t tmsg;
  while (wav_player_ui_dequeue(&msg) || ui_time_dequeue(&tmsg))
  {
    if (msg.type == WAV_UI_UPDATE_TIME)
    {
      set_var_current_song_time(msg.current_time);
      set_var_song_fulltime(msg.total_time);
      lv_bar_set_value(objects.music_process_bar, msg.percent, LV_ANIM_OFF);
      // ESP_LOGI("PER","当前百分比是%d",msg.percent) ;
    }
    else if (msg.type == WAV_UI_UPDATE_LRC)
    {
      set_var_current_lrc(msg.lrc);
    }
    else if (msg.type == WAV_UI_CLEAR_LRC)
    {
      set_var_current_lrc("");
    }

    // 消息
    if (tmsg.type == UI_TIME_UPDATE)
    {
      set_var_time_txt(tmsg.time_str);
    }
    else if (tmsg.type == UI_TIME_SYNC_OK)
    {
      set_var_cali_status("calied");
    }
    else if (tmsg.type == UI_TIME_SYNC_FAIL)
    {
      set_var_cali_status("failed");
    }else if(tmsg.type == UI_TIME_SYNCING){
       set_var_cali_status(tmsg.updating_msg);
    } 
  }

  ui_tick();
}

/* 主函数入口 */
void app_main(void)
{

  //   //  初始化录音
  //  init_mic_debug();

  wifi_time_sys_init();
  ui_time_queue_init(); // 创建时间队列

  wifi_time_set_ap("RX-Bridge", "RX3.14159");
  wifi_time_task_init(); // 持续执行更新函数

  sdcard_init();

  ESP_ERROR_CHECK(app_lcd_init());
  ESP_ERROR_CHECK(app_lvgl_init());
  ctp_init();

  aht20_init(s_bus);
  qmi8658_init(s_bus);

  ui_init();
  if (lvgl_port_lock(0))
  {
    lv_timer_create(lvgl_ui_timer_cb, 200, NULL);
    lvgl_port_unlock();
  }

  // 2. 初始化播放器
  int playlist_count = build_playlist_from_sd();
  if (playlist_count <= 0)
  {
    ESP_LOGW(TAG, "No .wav files found on SD card");
    // return;
  }

  esp_err_t ret = wav_player_init(s_playlist, 0.00f); // 音量 50%
  if (ret != ESP_OK)
  {
    ESP_LOGE("main", "Failed to init wav player");
    // return;
  }

  // xTaskCreate(record_to_sd_task, "record_to_sd_task", 8192, NULL, 5, NULL);

  // wav_player_send_cmd(WAV_CMD_STOP);
  // 初始化播放器之后立刻停止

  int num_files = scan_files_by_extension(".wav");
  if (num_files > 0)
  {
    char (*files)[50] = get_filtered_files();
    ESP_LOGI(TAG, "Listing .jpg files:");
    for (int i = 0; i < num_files; i++)
    {
      printf("  [%d] %s\n", i, files[i]);
    }
  }
  else
  {
    ESP_LOGW(TAG, "No .wav files found.");
  }

  aht20_start_task();   // 初始化aht20
  qmi8658_start_task(); // 初始化加速度

  draw_cube_start_angle_task(); // 持续运行重绘

  ui_time_timer_start(); // 开启定时器
}

// 可以使用这种命令来进行播放的切换等等

//    wav_player_send_cmd(WAV_CMD_STOP);
