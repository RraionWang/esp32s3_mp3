#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *music_page;
    lv_obj_t *timer_page;
    lv_obj_t *timer_running_page;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
    lv_obj_t *obj2;
    lv_obj_t *obj3;
    lv_obj_t *obj4;
    lv_obj_t *obj5;
    lv_obj_t *obj6;
    lv_obj_t *vol_bar;
    lv_obj_t *obj7;
    lv_obj_t *obj8;
    lv_obj_t *obj9;
    lv_obj_t *music_process_bar;
    lv_obj_t *lab_lrc;
    lv_obj_t *vol_value_lab;
    lv_obj_t *music_list;
    lv_obj_t *obj10;
    lv_obj_t *obj11;
    lv_obj_t *obj12;
    lv_obj_t *obj13;
    lv_obj_t *obj14;
    lv_obj_t *obj15;
    lv_obj_t *obj16;
    lv_obj_t *roller_min;
    lv_obj_t *roller_sec;
    lv_obj_t *roller_min_1;
    lv_obj_t *obj17;
    lv_obj_t *obj18;
    lv_obj_t *obj19;
    lv_obj_t *obj20;
    lv_obj_t *timer_indicator;
    lv_obj_t *obj21;
    lv_obj_t *obj22;
    lv_obj_t *obj23;
    lv_obj_t *obj24;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_MUSIC_PAGE = 2,
    SCREEN_ID_TIMER_PAGE = 3,
    SCREEN_ID_TIMER_RUNNING_PAGE = 4,
};

void create_screen_main();
void tick_screen_main();

void create_screen_music_page();
void tick_screen_music_page();

void create_screen_timer_page();
void tick_screen_timer_page();

void create_screen_timer_running_page();
void tick_screen_timer_running_page();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/