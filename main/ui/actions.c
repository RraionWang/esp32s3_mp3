#include "actions.h"
#include "screens.h"
#include "sd.h"
#include "string.h"
#include "esp_log.h"
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include "stdio.h"
#include "vars.h"
#include "audio.h"
#include <stdint.h>
#include "fonts.h"
#include "mytimer/mytimer.h"
#include "record/record.h"
#include "qmi.h"
#include "draw_cube.h"
#include "wifi_time.h"
#include "aht20.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bt/bt.h"

extern TaskHandle_t aht20TaskHandle;
extern TaskHandle_t qmiTubeTaskHandle;
extern TaskHandle_t drawTubeTaskHandle;


static bool music_list_updated = false;

static void music_list_btn_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);                     // 当前获得焦点的按钮
    lv_obj_t *list = lv_obj_get_parent(lv_obj_get_parent(btn)); // 按钮的爷爷是 List 对象

    // 1. 获取按钮上的文件名
    const char *file_name = lv_list_get_button_text(list, btn);

    if (file_name)
    {
        // 2. 拼接完整路径
        char full_path[128];
        snprintf(full_path, sizeof(full_path), "/sdcard/%s", file_name);

        ESP_LOGI("wav", "点击了: %s", full_path);
        wav_player_play_index(index);

        // 打印之后进行隐藏
        lv_obj_update_flag(objects.music_list, LV_OBJ_FLAG_HIDDEN, true);
    }
}

void action_update_list(lv_event_t *e)
{

    if (!music_list_updated)
    {
        music_list_updated = true;

        int num_files = scan_files_by_extension(".wav");

        if (num_files > 0)
        {

            lv_obj_clean(objects.music_list);
            char (*files)[50] = get_filtered_files();
            ESP_LOGI("MUSIC_LIST", "Listing .wav files:");
            for (int i = 0; i < num_files; i++)
            {
                ESP_LOGI("MUSIC_LIST", "  [%d] %s\n", i, files[i]);
                lv_obj_t *btn = lv_list_add_button(objects.music_list, LV_SYMBOL_AUDIO, files[i]);
                lv_obj_t *label = lv_obj_get_child(btn, 1);
                if (label)
                {
                    lv_obj_set_style_text_font(label, &ui_font_cn_font_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                lv_obj_add_event_cb(btn, music_list_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            }
        }
        else
        {
            ESP_LOGE("MUSIC_LIST", "No .wav files found.");
        }
    }

    lv_obj_update_flag(objects.music_list, LV_OBJ_FLAG_HIDDEN, false);
}

void action_hide_music_list(lv_event_t *e)
{
    // TODO: Implement action hide_music_list here
}

// 设置音量

void action_set_mp3_vol(lv_event_t *e)
{
    // TODO: Implement action set_mp3_vol here

    ESP_LOGI("VOL", "设置音量为%3f", 0.01f * get_var_vol());
    wav_player_set_volume_cmd(0.01f * get_var_vol());
}

void action_play_next_song(lv_event_t *e)
{
    // TODO: Implement action play_next_song here

    ESP_LOGI("TAG", "播放下一曲");
    wav_player_send_cmd(WAV_CMD_NEXT);
}

void action_play_prev_song(lv_event_t *e)
{
    // TODO: Implement action play_prev_song here
    ESP_LOGI("TAG", "播放上一曲");
    wav_player_send_cmd(WAV_CMD_PREV);
}

void action_song_continue(lv_event_t *e)
{
    // TODO: Implement action song_continue here

    ESP_LOGI("TAG", "继续播放");
    wav_player_send_cmd(WAV_CMD_RESUME);
}

void action_song_pause(lv_event_t *e)
{
    // TODO: Implement action song_pause here
    wav_player_send_cmd(WAV_CMD_PAUSE);
    ESP_LOGI("TAG", "暂停播放");
}

static uint64_t s_passed_sec = 0;
static uint64_t s_total_sec = 0;

static char time_txt[128];

static void timer_cb(uint64_t count)
{
    s_passed_sec++;

    if (s_passed_sec > s_total_sec)
    {
        mytimer_stop();
        return;
    }

    uint64_t sec_left = s_total_sec - s_passed_sec;

    int hr = sec_left / 3600;
    int min = (sec_left % 3600) / 60;
    int sec = sec_left % 60;

    set_var_current_hr_cnt(hr);
    set_var_current_min_cnt(min);
    set_var_current_sec_cnt(sec);

    snprintf(
        time_txt,
        sizeof(time_txt),

        "%02d:%02d:%02d\n",
        hr, min, sec);

    set_var_current_time_text(time_txt);

    ESP_LOGI("TIMER", "执行回调 %02d:%02d:%02d", hr, min, sec);

    // 设置总计时变量
    set_var_current_timer_cnt(sec_left);
    // lv_arc_set_value(objects.timer_indicator,sec_left) ;
}

void action_start_timer(lv_event_t *e)
{
    s_total_sec = get_var_totol_timer_sec();
    s_passed_sec = 0;

    int hr = s_total_sec / 3600;
    int min = (s_total_sec % 3600) / 60;
    int sec = s_total_sec % 60;

    set_var_current_hr_cnt(hr);
    set_var_current_min_cnt(min);
    set_var_current_sec_cnt(sec);

    snprintf(
        time_txt,
        sizeof(time_txt),

        "%02d:%02d:%02d\n",
        hr, min, sec);

    set_var_current_time_text(time_txt);

    mytimer_start(1000000, timer_cb);
}

void action_stop_timer(lv_event_t *e)
{

    mytimer_stop();

    s_passed_sec = 0;
    s_total_sec = 0;

    // TODO: Implement action stop_timer here
}

// 暂停录音
void action_record_stop(lv_event_t *e)
{
    ESP_LOGI("REC", "停止录音");
    //  record_stop();
    // TODO: Implement action record_stop here
}

// 开始录音

void action_record_start(lv_event_t *e)
{

    ESP_LOGI("REC", "开始录音");
    //   record_start("/sdcard/rec_001.wav");

    // TODO: Implement action record_start here
}

char current_time_text[100] = {0};

const char *get_var_current_time_text()
{
    return current_time_text;
}

void set_var_current_time_text(const char *value)
{
    strncpy(current_time_text, value, sizeof(current_time_text) / sizeof(char));
    current_time_text[sizeof(current_time_text) / sizeof(char) - 1] = 0;
}

// 生成立方体
void action_gen_cube_widget(lv_event_t *e)
{

    // if(eTaskGetState(aht20TaskHandle) == eSuspended){
    //     vTaskResume(aht20TaskHandle) ; 
    // }

      

    // if( eTaskGetState(qmiTubeTaskHandle)==eSuspended){
    //       vTaskResume(qmiTubeTaskHandle) ; 
    // }



    draw_cube_create(objects.cord_obj);


    // if( eTaskGetState(qmiTubeTaskHandle)==eSuspended && eTaskGetState(drawTubeTaskHandle) == eRunning){
    //     vTaskSuspend(qmiTubeTaskHandle) ; 
    // }



}

// 销毁立方体
void action_destroy_cube_widget(lv_event_t *e)


{

    // if(eTaskGetState(aht20TaskHandle) == eRunning){
    //     vTaskSuspend(aht20TaskHandle) ; 
    // }

    draw_cube_destroy();


    //     if(eTaskGetState(qmiTubeTaskHandle) == eRunning){
    //     vTaskSuspend(qmiTubeTaskHandle) ; 
    // }

    // if(eTaskGetState(qmiTubeTaskHandle) == eSuspended && eTaskGetState(drawTubeTaskHandle)==eRunning){
    //       vTaskSuspend(drawTubeTaskHandle) ; 
    // }



}

// 校准时间





// // 全局信号量
// SemaphoreHandle_t s_time_sync_sem = NULL;

// static void wifi_time_task(void *pvParameters) {
//     while (1) {
//         // 使用 portMAX_DELAY 确保在没有信号量时任务处于阻塞态，完全不占 CPU
//         if (xSemaphoreTake(s_time_sync_sem, portMAX_DELAY) == pdTRUE) {
//             wifi_time_sync_now();
//         }
//     }
// }

// // 设置为比较不重要的
// void wifi_time_task_init(void)
// {
//     s_time_sync_sem = xSemaphoreCreateBinary();

//     xTaskCreatePinnedToCore(
//         wifi_time_task,
//         "wifi_time_task",
//         8192, // WiFi 必须大栈
//         NULL,
//         1,
//         NULL,
//         1 // Core 0（WiFi 必须）
//     );
// }




void action_cali_time(lv_event_t *e)
{
    if (s_time_sync_sem)
    {
        xSemaphoreGive(s_time_sync_sem);
    }


        // wifi_time_sync_now(); // 目前使用信号会出现音乐无法播放的问题

}



// 生成菜单


static lv_obj_t * root_page;

/* ================= 创建一个最简单的文本条目 ================= */
static lv_obj_t * menu_item(lv_obj_t * parent, const char * icon, const char * txt)
{
    lv_obj_t * cont = lv_menu_cont_create(parent);

    if(icon) {
        lv_obj_t * img = lv_image_create(cont);
        lv_image_set_src(img, icon);
    }

    lv_obj_t * label = lv_label_create(cont);
    lv_label_set_text(label, txt);
    lv_obj_set_flex_grow(label, 1);

    return cont;
}



static void menu_back_cb(lv_event_t * e)
{
    lv_obj_t * menu = lv_event_get_target(e);

    /* 是否在 root page */
   

        /* 隐藏 menu */
         lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);

     
        lv_scr_load(objects.main);
    
}

 static lv_obj_t * wifi_page;
 static lv_obj_t * wifi_sec ;
 static lv_obj_t * sys_page ; 
  static lv_obj_t * bluetooth_page ; 




static void scan_bt_cb(lv_event_t * e)
{
    ESP_LOGI("BTN","按下按键了"); 
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {

        if(bt_task_handle) {
            xTaskNotify(bt_task_handle, 0, eNoAction);
        }
    }
}



static bool is_menu_gened =false ; 
void action_gen_setting_meu(lv_event_t *e) {

    lv_obj_remove_flag(objects.setting_menu , LV_OBJ_FLAG_HIDDEN);

    if(!is_menu_gened){
        is_menu_gened = true ;
    
    // TODO: Implement action gen_setting_meu here

    /* 创建 menu */
    lv_obj_t * menu = objects.setting_menu ; 

    lv_obj_set_size(menu, LV_PCT(100), LV_PCT(100));

    if(root_page==NULL){

          

    

    lv_menu_set_mode_root_back_button(menu, LV_MENU_ROOT_BACK_BUTTON_ENABLED);
    lv_obj_add_event_cb(menu, menu_back_cb, LV_EVENT_CLICKED, NULL);


    /* ================= WiFi 页面 ================= */
    wifi_page = lv_menu_page_create(menu, "WiFi ");
    wifi_sec  = lv_menu_section_create(wifi_page);

    lv_label_create(wifi_sec);
    lv_label_set_text(lv_obj_get_child(wifi_sec, 0), "SSID: MyWifi\nIP: 192.168.1.10");


        /* ================= 蓝牙信息页面 ================= */
    bluetooth_page = lv_menu_page_create(menu, "info");
    lv_obj_t * bluetooth_sec  = lv_menu_section_create(bluetooth_page);

    lv_label_create(bluetooth_sec);
    lv_label_set_text(lv_obj_get_child(bluetooth_sec, 0),
                      "bluetooth");
    
    lv_obj_t* scan_bt = lv_button_create(bluetooth_sec) ;
    lv_obj_t * label = lv_label_create(scan_bt);
    lv_label_set_text(label, "Scan Bluetooth");
    lv_obj_center(label);
lv_obj_add_event_cb(scan_bt, scan_bt_cb, LV_EVENT_CLICKED, NULL);
       



    /* ================= 系统信息页面 ================= */
     sys_page = lv_menu_page_create(menu, "info");
    lv_obj_t * sys_sec  = lv_menu_section_create(sys_page);

    lv_label_create(sys_sec);
    lv_label_set_text(lv_obj_get_child(sys_sec, 0),
                      "Device: ESP32-S3\nFW: v1.0\nBuild: 2026-01-13");


           

    /* ================= 侧边栏 Root 页面 ================= */
    root_page = lv_menu_page_create(menu, "Setting");
    lv_obj_t * root_sec = lv_menu_section_create(root_page);

    lv_obj_t * item;

    item = menu_item(root_sec, LV_SYMBOL_WIFI, "WiFi");
    lv_menu_set_load_page_event(menu, item, wifi_page);

    item = menu_item(root_sec, LV_SYMBOL_BLUETOOTH, "bluetooth");
    lv_menu_set_load_page_event(menu, item, bluetooth_page);


    item = menu_item(root_sec, LV_SYMBOL_SETTINGS, "info");
    lv_menu_set_load_page_event(menu, item, sys_page);



    /* 设置侧边栏 */
    lv_menu_set_sidebar_page(menu, root_page);

    /* 默认选中第一个 */
    lv_obj_send_event(item, LV_EVENT_CLICKED, NULL);
    
    }
}
}





// 重新设置wifi


void action_reset_wifi(lv_event_t *e) {
    // TODO: Implement action reset_wifi here

    set_var_ssid_txt("重新配网中...");
    set_var_password_txt("重新配网中");

   
    // 使用通知

        if (wifi_ctrl_task) {
        xTaskNotifyGive(wifi_ctrl_task);
    }

}




char ssid_txt[100] = { 0 };

const char *get_var_ssid_txt() {
    return ssid_txt;
}

void set_var_ssid_txt(const char *value) {
    strncpy(ssid_txt, value, sizeof(ssid_txt) / sizeof(char));
    ssid_txt[sizeof(ssid_txt) / sizeof(char) - 1] = 0;
}




char password_txt[100] = { 0 };

const char *get_var_password_txt() {
    return password_txt;
}

void set_var_password_txt(const char *value) {
    strncpy(password_txt, value, sizeof(password_txt) / sizeof(char));
    password_txt[sizeof(password_txt) / sizeof(char) - 1] = 0;
}



