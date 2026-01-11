#include "draw_cube.h"
#include <math.h>
#include <stdlib.h>
#include "vars.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

/* ================= 尺寸 ================= */
#define CUBE_W 130
#define CUBE_H 130

/* ================= Canvas ================= */
static lv_obj_t *s_canvas = NULL;

/* Canvas draw buffer（官方示例写法） */
LV_DRAW_BUF_DEFINE_STATIC(s_draw_buf, CUBE_W, CUBE_H, LV_COLOR_FORMAT_ARGB8888);

/* ================= 姿态角（弧度） ================= */
static float s_pitch = 0.0f;
static float s_roll  = 0.0f;

/* ================= 3D 数据 ================= */
typedef struct {
    float x, y, z;
} vec3_t;

static const vec3_t cube_pts[8] = {
    {-1,-1,-1}, { 1,-1,-1}, { 1, 1,-1}, {-1, 1,-1},
    {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1},
};

static const uint8_t cube_edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
};

/* ================= 创建 ================= */
void draw_cube_create(lv_obj_t *parent)
{
    if(s_canvas) return;

    LV_DRAW_BUF_INIT_STATIC(s_draw_buf);

    s_canvas = lv_canvas_create(parent);
    lv_obj_set_size(s_canvas, CUBE_W, CUBE_H);
    lv_obj_center(s_canvas);

    lv_canvas_set_draw_buf(s_canvas, &s_draw_buf);
    lv_canvas_fill_bg(s_canvas, lv_color_white(), LV_OPA_COVER);

    draw_cube_redraw();
}

/* ================= 销毁 ================= */
void draw_cube_destroy(void)
{
    if(s_canvas) {
        lv_obj_del(s_canvas);
        s_canvas = NULL;
    }
}

/* ================= 设置角度 ================= */
void draw_cube_set_angle(float pitch_deg, float roll_deg)
{

    s_pitch = pitch_deg * 0.017453292f;
    s_roll  = roll_deg  * 0.017453292f;

    draw_cube_redraw();
}

/* ================= 重绘 ================= */
void draw_cube_redraw(void)
{
    if(!s_canvas) return;

    /* 清屏 */
    lv_canvas_fill_bg(s_canvas, lv_color_white(), LV_OPA_COVER);

    /* 初始化 layer（官方示例） */
    lv_layer_t layer;
    lv_canvas_init_layer(s_canvas, &layer);

    /* 线条描述符 */
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_black();
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    const int cx = CUBE_W / 2;
    const int cy = CUBE_H / 2;
    const int scale = CUBE_W / 3;

    vec3_t p[8];

    /* 旋转 */
    for(int i = 0; i < 8; i++) {
        p[i] = cube_pts[i];

        /* pitch (X) */
        float y = p[i].y * cosf(s_pitch) - p[i].z * sinf(s_pitch);
        float z = p[i].y * sinf(s_pitch) + p[i].z * cosf(s_pitch);
        p[i].y = y; p[i].z = z;

        /* roll (Y) */
        float x =  p[i].x * cosf(s_roll) + p[i].z * sinf(s_roll);
        z       = -p[i].x * sinf(s_roll) + p[i].z * cosf(s_roll);
        p[i].x = x; p[i].z = z;
    }

    /* 画 12 条边 */
    for(int i = 0; i < 12; i++) {
        vec3_t a = p[cube_edges[i][0]];
        vec3_t b = p[cube_edges[i][1]];

        dsc.p1.x = (int)(a.x * scale) + cx;
        dsc.p1.y = (int)(a.y * scale) + cy;
        dsc.p2.x = (int)(b.x * scale) + cx;
        dsc.p2.y = (int)(b.y * scale) + cy;

        lv_draw_line(&layer, &dsc);
    }

    /* 提交 layer（官方示例） */
    lv_canvas_finish_layer(s_canvas, &layer);
}


static void draw_cube_angle_task(void *arg)
{
    (void)arg;

    while (1) {
        /* 从 vars 读取角度（单位：度） */
        float pitch = get_var_pitch_var();
        float roll  = get_var_roll_var();

        /* ⚠️ LVGL 操作一定要在 lock 内 */
        lvgl_port_lock(0);

        draw_cube_set_angle(pitch, roll);

        lvgl_port_unlock();

        /* 20~30Hz 刷新就很顺滑 */
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}


void draw_cube_start_angle_task(void)
{
    xTaskCreate(
        draw_cube_angle_task,
        "cube_angle_task",
        4096,
        NULL,
        4,      // UI 类任务，优先级不需要太高
        NULL
    );
}

