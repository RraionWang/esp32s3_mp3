#include "vars.h"
#include "string.h"
#include "esp_log.h"

bool is_playing;

bool get_var_is_playing() {
    return is_playing;
}

void set_var_is_playing(bool value) {
    is_playing = value;
}



char song_fulltime[100] = { 0 };

const char *get_var_song_fulltime() {
    return song_fulltime;
}

void set_var_song_fulltime(const char *value) {

    if(strcmp(get_var_song_fulltime(),value)==0){
        return ; 
    }



    strncpy(song_fulltime, value, sizeof(song_fulltime) / sizeof(char));
    song_fulltime[sizeof(song_fulltime) / sizeof(char) - 1] = 0;
}


char current_song_time[100] = { 0 };

const char *get_var_current_song_time() {
    return current_song_time;
}

void set_var_current_song_time(const char *value) {


        // 如果相同就不修改
    if(strcmp(get_var_current_song_time(),value)==0){
        return ; 
    }



    strncpy(current_song_time, value, sizeof(current_song_time) / sizeof(char));
    current_song_time[sizeof(current_song_time) / sizeof(char) - 1] = 0;
}







// 设置歌词的代码

char current_lrc[100] = { 0 };

const char *get_var_current_lrc() {
    return current_lrc;
}

void set_var_current_lrc(const char *value) {


    // 如果相同就不修改
    if(strcmp(get_var_current_lrc(),value)==0){
        ESP_LOGI("LRC","歌词相同不修改") ; 
        
        return ; 
    }


    strncpy(current_lrc, value, sizeof(current_lrc) / sizeof(char));
    current_lrc[sizeof(current_lrc) / sizeof(char) - 1] = 0;
}



// 设置音量

int32_t vol;

int32_t get_var_vol() {
    return vol;
}

void set_var_vol(int32_t value) {
    vol = value;
}




// 设置当前时间
int32_t current_timer_cnt;

int32_t get_var_current_timer_cnt() {
    return current_timer_cnt;
}

void set_var_current_timer_cnt(int32_t value) {
    current_timer_cnt = value;
}



int32_t current_hr_cnt;

int32_t get_var_current_hr_cnt() {
    return current_hr_cnt;
}

void set_var_current_hr_cnt(int32_t value) {
    current_hr_cnt = value;
}


int32_t current_min_cnt;

int32_t get_var_current_min_cnt() {
    return current_min_cnt;
}

void set_var_current_min_cnt(int32_t value) {
    current_min_cnt = value;
}



int32_t current_sec_cnt;

int32_t get_var_current_sec_cnt() {
    return current_sec_cnt;
}

void set_var_current_sec_cnt(int32_t value) {
    current_sec_cnt = value;
}




// 总时间
int32_t totol_timer_sec;

int32_t get_var_totol_timer_sec() {
    return totol_timer_sec;
}

void set_var_totol_timer_sec(int32_t value) {
    totol_timer_sec = value;
}


// 录音标志

bool is_recording;

bool get_var_is_recording() {
    return is_recording;
}

void set_var_is_recording(bool value) {
    is_recording = value;
}



// qmi 文本
char qmi_text[100] = { 0 };

const char *get_var_qmi_text() {
    return qmi_text;
}

void set_var_qmi_text(const char *value) {
    strncpy(qmi_text, value, sizeof(qmi_text) / sizeof(char));
    qmi_text[sizeof(qmi_text) / sizeof(char) - 1] = 0;
}



// aht20 文本


char aht20_text[100] = { 0 };

const char *get_var_aht20_text() {
    return aht20_text;
}

void set_var_aht20_text(const char *value) {
    strncpy(aht20_text, value, sizeof(aht20_text) / sizeof(char));
    aht20_text[sizeof(aht20_text) / sizeof(char) - 1] = 0;
}



float pitch_var;

float get_var_pitch_var() {
    return pitch_var;
}

void set_var_pitch_var(float value) {
    pitch_var = value;
}


float roll_var;

float get_var_roll_var() {
    return roll_var;
}

void set_var_roll_var(float value) {
    roll_var = value;
}
