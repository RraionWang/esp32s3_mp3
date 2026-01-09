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
    FLOW_GLOBAL_VARIABLE_PLAY_TEXT = 0
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


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/