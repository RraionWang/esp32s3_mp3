#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_update_list(lv_event_t * e);
extern void action_hide_music_list(lv_event_t * e);
extern void action_set_mp3_vol(lv_event_t * e);
extern void action_play_next_song(lv_event_t * e);
extern void action_play_prev_song(lv_event_t * e);
extern void action_song_continue(lv_event_t * e);
extern void action_song_pause(lv_event_t * e);
extern void action_start_timer(lv_event_t * e);
extern void action_stop_timer(lv_event_t * e);
extern void action_record_stop(lv_event_t * e);
extern void action_record_start(lv_event_t * e);
extern void action_gen_cube_widget(lv_event_t * e);
extern void action_destroy_cube_widget(lv_event_t * e);
extern void action_cali_time(lv_event_t * e);
extern void action_gen_setting_meu(lv_event_t * e);
extern void action_reset_wifi(lv_event_t * e);


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/