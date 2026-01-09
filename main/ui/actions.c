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
                lv_obj_t *label = lv_obj_get_child(btn,1) ; 
                if(label){
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

void action_set_mp3_vol(lv_event_t *e) {
    // TODO: Implement action set_mp3_vol here

         ESP_LOGI("VOL","设置音量为%3f",0.01f * get_var_vol() )  ; 
        wav_player_set_volume_cmd(0.01f * get_var_vol() ) ;
   
}



void action_play_next_song(lv_event_t *e) {
    // TODO: Implement action play_next_song here

    ESP_LOGI("TAG","播放下一曲") ;
      wav_player_send_cmd(WAV_CMD_NEXT);


}



void action_play_prev_song(lv_event_t *e) {
    // TODO: Implement action play_prev_song here
        ESP_LOGI("TAG","播放上一曲") ;
             wav_player_send_cmd(WAV_CMD_PREV);

}


void action_song_continue(lv_event_t *e) {
    // TODO: Implement action song_continue here

        ESP_LOGI("TAG","继续播放") ;
        wav_player_send_cmd(WAV_CMD_RESUME);

}


void action_song_pause(lv_event_t *e) {
    // TODO: Implement action song_pause here
     wav_player_send_cmd(WAV_CMD_PAUSE);
        ESP_LOGI("TAG","暂停播放") ;
}
