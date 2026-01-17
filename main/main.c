
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
#include "bt/bt.h"
#include "bat/bat.h"

static const char *TAG = "main";

EXT_RAM_BSS_ATTR const char *s_playlist[MAX_FILES + 1];
EXT_RAM_BSS_ATTR char s_playlist_paths[MAX_FILES][128];

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

  while (wav_player_ui_dequeue(&msg))
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
  }

  ui_tick();
}

static void lvgl_ui_wifitimer_timer_cb(lv_timer_t *timer)
{
  (void)timer;

  ui_time_msg_t tmsg;
  while (ui_time_dequeue(&tmsg))
  {

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
    }
    else if (tmsg.type == UI_TIME_SYNCING)
    {
      set_var_cali_status(tmsg.updating_msg);
    }
  }

  ui_tick();
}

#include "esp_heap_caps.h"

void print_ram_info()
{
  // 1. 内部 RAM (速度快，但容量小，通常给 DMA 或关键任务用)
  size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

  // 2. 外部 PSRAM (容量大，速度稍慢，存放 UI 图片、大型缓冲区)
  size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  // 3. 历史上曾经出现过的最低剩余内存 (水位线，用于检查是否接近崩溃)
  size_t min_free = esp_get_minimum_free_heap_size();

  ESP_LOGI("ram", "\n--- 内存状态报告 ---\n");
  ESP_LOGI("ram", "内部 RAM 剩余: %d KB\n", internal_free / 1024);
  set_var_current_sram_used(512 - internal_free / 1024);
  ESP_LOGI("ram", "外部 PSRAM 剩余: %d KB\n", psram_free / 1024);
  set_var_current_psram_used(8192 - psram_free / 1024);
  ESP_LOGI("ram", "历史最低水位线: %d KB\n", min_free / 1024);
  ESP_LOGI("ram", "--------------------\n");
}

static void print_mem_task(void *arg)
{

  while (1)
  {
    print_ram_info();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


// static void wifi_prov_task(void *arg)
// {
//     wifi_prov_run("ESP32S3", "123456789");
//     vTaskDelete(NULL);   // 配完就销毁
// }



/* 主函数入口 */
void app_main(void)
{

  xTaskCreate(print_mem_task, "printmeme", 4096, NULL, 5, NULL);

    sdcard_init();


  //   //  初始化录音
  //  init_mic_debug();
  // 目前录音电路有问题 废弃

  // wifi_time_sys_init();
  // ui_time_queue_init(); // 创建时间队列

  // wifi_reprovision_task_init() ; // 启动重复配网检测程序

  //  xTaskCreate(
  //       wifi_prov_task,
  //       "wifi_prov",
  //       8192 * 2,
  //       NULL,
  //       4,
  //       NULL
  //   );
    


  // wifi_time_task_init(); // 持续执行更新函数



  ESP_ERROR_CHECK(app_lcd_init());
  ESP_ERROR_CHECK(app_lvgl_init());
  ctp_init();

  aht20_init(s_bus);
  qmi8658_init(s_bus);

  ui_init();

  if (lvgl_port_lock(0))
  {
    lv_timer_create(lvgl_ui_timer_cb, 200, NULL); //

    lvgl_port_unlock();
  }

  // if (lvgl_port_lock(0))
  // {

  //   lv_timer_create(lvgl_ui_wifitimer_timer_cb, 500, NULL);
  //   lvgl_port_unlock();
  // }

  ui_time_timer_start(); // 开启定时器

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

  // bt_init(); // 初始化蓝牙
  // bt_task(); // 开启蓝牙任务

  // // 打印内存信息


   /* 1. 初始化 ADC */
    adc_gpio1_init();

    /* 2. 创建 ADC 任务 */
    xTaskCreate(
        adc_gpio1_task,
        "adc_gpio1_task",
        4096,
        NULL,
        5,
        NULL
    );


     chart_voltage_init();

    xTaskCreate(
        chart_voltage_task,
        "chart_voltage_task",
        4096,
        NULL,
        5,
        NULL
    );


charge_state_init();

    xTaskCreate(
        charge_state_task,
        "charge_state_task",
        2048,
        NULL,
        5,
        NULL
    );


//      init_mic_debug();

//  xTaskCreate(record_to_sd_task, "record_to_sd_task", 8192, NULL, 5, NULL);
recorder_init();        // 开机调用一次
}

// 可以使用这种命令来进行播放的切换等等

//    wav_player_send_cmd(WAV_CMD_STOP);
