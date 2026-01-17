#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations



// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_PLAY_TEXT = 0,
    FLOW_GLOBAL_VARIABLE_TIMER_HR_CNT = 1,
    FLOW_GLOBAL_VARIABLE_TIMER_MIN_CNT = 2,
    FLOW_GLOBAL_VARIABLE_TIMER_SEC_CNT = 3,
    FLOW_GLOBAL_VARIABLE_RECORD_TEXT = 4
};

// Native global variables

extern bool get_var_is_playing();
extern void set_var_is_playing(bool value);
extern const char *get_var_song_fulltime();
extern void set_var_song_fulltime(const char *value);
extern const char *get_var_current_song_time();
extern void set_var_current_song_time(const char *value);
extern const char *get_var_current_lrc();
extern void set_var_current_lrc(const char *value);
extern int32_t get_var_vol();
extern void set_var_vol(int32_t value);
extern int32_t get_var_totol_timer_sec();
extern void set_var_totol_timer_sec(int32_t value);
extern int32_t get_var_current_timer_cnt();
extern void set_var_current_timer_cnt(int32_t value);
extern int32_t get_var_current_hr_cnt();
extern void set_var_current_hr_cnt(int32_t value);
extern int32_t get_var_current_min_cnt();
extern void set_var_current_min_cnt(int32_t value);
extern int32_t get_var_current_sec_cnt();
extern void set_var_current_sec_cnt(int32_t value);
extern bool get_var_is_recording();
extern void set_var_is_recording(bool value);
extern const char *get_var_qmi_text();
extern void set_var_qmi_text(const char *value);
extern const char *get_var_aht20_text();
extern void set_var_aht20_text(const char *value);
extern const char *get_var_current_time_text();
extern void set_var_current_time_text(const char *value);
extern float get_var_pitch_var();
extern void set_var_pitch_var(float value);
extern float get_var_roll_var();
extern void set_var_roll_var(float value);
extern const char *get_var_time_txt();
extern void set_var_time_txt(const char *value);
extern const char *get_var_cali_status();
extern void set_var_cali_status(const char *value);
extern int32_t get_var_current_sram_used();
extern void set_var_current_sram_used(int32_t value);
extern int32_t get_var_current_psram_used();
extern void set_var_current_psram_used(int32_t value);
extern const char *get_var_ssid_txt();
extern void set_var_ssid_txt(const char *value);
extern const char *get_var_password_txt();
extern void set_var_password_txt(const char *value);
extern const char *get_var_wifi_logo();
extern void set_var_wifi_logo(const char *value);
extern const char *get_var_bat_voltage();
extern void set_var_bat_voltage(const char *value);


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/