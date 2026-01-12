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
    lv_obj_t *record_page;
    lv_obj_t *sensor_page;
    lv_obj_t *bat_page;
    lv_obj_t *time_page;
    lv_obj_t *ram_page;
    lv_obj_t *setting_page;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
    lv_obj_t *obj2;
    lv_obj_t *obj3;
    lv_obj_t *obj4;
    lv_obj_t *obj5;
    lv_obj_t *obj6;
    lv_obj_t *obj7;
    lv_obj_t *obj8;
    lv_obj_t *obj9;
    lv_obj_t *obj10;
    lv_obj_t *obj11;
    lv_obj_t *obj12;
    lv_obj_t *vol_bar;
    lv_obj_t *obj13;
    lv_obj_t *obj14;
    lv_obj_t *obj15;
    lv_obj_t *obj16;
    lv_obj_t *obj17;
    lv_obj_t *obj18;
    lv_obj_t *obj19;
    lv_obj_t *obj20;
    lv_obj_t *cali_btn;
    lv_obj_t *obj21;
    lv_obj_t *music_process_bar;
    lv_obj_t *lab_lrc;
    lv_obj_t *vol_value_lab;
    lv_obj_t *music_list;
    lv_obj_t *obj22;
    lv_obj_t *obj23;
    lv_obj_t *obj24;
    lv_obj_t *obj25;
    lv_obj_t *obj26;
    lv_obj_t *obj27;
    lv_obj_t *obj28;
    lv_obj_t *obj29;
    lv_obj_t *roller_min;
    lv_obj_t *roller_sec;
    lv_obj_t *roller_min_1;
    lv_obj_t *obj30;
    lv_obj_t *obj31;
    lv_obj_t *timer_indicator;
    lv_obj_t *obj32;
    lv_obj_t *obj33;
    lv_obj_t *record_chart;
    lv_obj_t *obj34;
    lv_obj_t *obj35;
    lv_obj_t *cord_obj;
    lv_obj_t *obj36;
    lv_obj_t *obj37;
    lv_obj_t *obj38;
    lv_obj_t *bat_chart;
    lv_obj_t *obj39;
    lv_obj_t *obj40;
    lv_obj_t *obj41;
    lv_obj_t *obj42;
    lv_obj_t *ram_bar;
    lv_obj_t *psram_bar;
    lv_obj_t *obj43;
    lv_obj_t *obj44;
    lv_obj_t *obj45;
    lv_obj_t *setting_menu;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_MUSIC_PAGE = 2,
    SCREEN_ID_TIMER_PAGE = 3,
    SCREEN_ID_TIMER_RUNNING_PAGE = 4,
    SCREEN_ID_RECORD_PAGE = 5,
    SCREEN_ID_SENSOR_PAGE = 6,
    SCREEN_ID_BAT_PAGE = 7,
    SCREEN_ID_TIME_PAGE = 8,
    SCREEN_ID_RAM_PAGE = 9,
    SCREEN_ID_SETTING_PAGE = 10,
};

void create_screen_main();
void tick_screen_main();

void create_screen_music_page();
void tick_screen_music_page();

void create_screen_timer_page();
void tick_screen_timer_page();

void create_screen_timer_running_page();
void tick_screen_timer_running_page();

void create_screen_record_page();
void tick_screen_record_page();

void create_screen_sensor_page();
void tick_screen_sensor_page();

void create_screen_bat_page();
void tick_screen_bat_page();

void create_screen_time_page();
void tick_screen_time_page();

void create_screen_ram_page();
void tick_screen_ram_page();

void create_screen_setting_page();
void tick_screen_setting_page();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/