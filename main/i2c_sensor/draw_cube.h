#pragma once
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


void draw_cube_create(lv_obj_t *parent);
void draw_cube_destroy(void);
void draw_cube_set_angle(float pitch_deg, float roll_deg);
void draw_cube_redraw(void);
void draw_cube_start_angle_task(void);

extern TaskHandle_t drawTubeTaskHandle  ; 
