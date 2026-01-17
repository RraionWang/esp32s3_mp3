#include "bat.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_lvgl_port.h"
#include "vars.h"
#include "driver/gpio.h"

static const char *TAG = "ADC_GPIO1";

/* GPIO1 -> ADC1_CHANNEL_0 (ESP32-S3) */
#define ADC_UNIT_ID   ADC_UNIT_1
#define ADC_CHANNEL  ADC_CHANNEL_0

/* ADC 句柄 */
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t         s_cali_handle = NULL;
static bool                      s_cali_enable = false;

#include "screens.h"
static lv_obj_t *s_chart = NULL;
static lv_chart_series_t *s_series = NULL;


/* ================= 初始化 ================= */
void adc_gpio1_init(void)
{
    esp_err_t ret;

    /* 1. ADC oneshot unit */
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = ADC_UNIT_ID,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc_handle));

    /* 2. ADC channel config */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT, // 12bit
        .atten    = ADC_ATTEN_DB_11,       // 0~3.3V
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(
        s_adc_handle, ADC_CHANNEL, &chan_cfg));

    /* 3. ADC 校准（强烈推荐） */
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_ID,
        .chan     = ADC_CHANNEL,
        .atten    = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);
    if (ret == ESP_OK) {
        s_cali_enable = true;
        ESP_LOGI(TAG, "ADC 校准成功");
    } else {
        ESP_LOGW(TAG, "ADC 校准失败，仅输出 raw");
    }

  


}



 int voltage_mv ;

/* ================= 采样任务 ================= */
void adc_gpio1_task(void *arg)
{
    int raw = 0;
   

    ESP_LOGI(TAG, "ADC GPIO1 task started");

    while (1) {
        ESP_ERROR_CHECK(
            adc_oneshot_read(s_adc_handle, ADC_CHANNEL, &raw));

        if (s_cali_enable) {
            adc_cali_raw_to_voltage(s_cali_handle, raw, &voltage_mv);
            ESP_LOGI(TAG, "GPIO1: raw=%d, voltage=%d mV",
                     raw, voltage_mv);

        
                

        } else {
            ESP_LOGI(TAG, "GPIO1: raw=%d", raw);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ===== LVGL 线程安全回调 ===== */
static void chart_async_update_cb(void *user_data)
{
    int value = (int)user_data;

    if (!s_chart || !s_series) return;

    lv_chart_set_next_value(s_chart, s_series, value);
     lv_chart_refresh(s_chart);
}

/* ================= 初始化 ================= */
void chart_voltage_init(void)
{
    /* 1. 创建 Chart */
    s_chart = lv_chart_create(objects.chart_container);
    lv_obj_set_size(s_chart, lv_pct(100), lv_pct(100));
    lv_obj_center(s_chart);

    /* 2. Chart 基本属性 */
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, 100);
    lv_chart_set_range(
        s_chart,
        LV_CHART_AXIS_PRIMARY_Y,
        3700,
        4200
    );

   
    
    /* 3. 添加曲线 */
    s_series = lv_chart_add_series(
        s_chart,
        lv_palette_main(LV_PALETTE_GREEN),
        LV_CHART_AXIS_PRIMARY_Y
    );

    ESP_LOGI(TAG, "Chart init OK");
}


int get_battery_voltage_mv(){
    return voltage_mv*3 ; 
}

void mv_to_v_str_2dec(int mv, char *out, size_t out_len)
{
    int v100 = mv / 10;   // mV -> ×100

    snprintf(
        out,
        out_len,
        "%d.%02dV",
        v100 / 100,
        v100 % 100
    );
}


/* ================= 实时绘制任务 ================= */
void chart_voltage_task(void *arg)
{
    ESP_LOGI(TAG, "Chart task started");

    while (1) {
        /* TODO：换成你真实的数据源 */


        int voltage_mv = get_battery_voltage_mv();  // 0~3300


         int v100 = voltage_mv / 10;   // mV -> ×100

    char voltage_str[8];

mv_to_v_str_2dec(voltage_mv, voltage_str, sizeof(voltage_str));

set_var_bat_voltage(voltage_str);



        /* 安全通知 LVGL 线程 */
        lv_async_call(
            chart_async_update_cb,
            (void *)voltage_mv
        );




        vTaskDelay(pdMS_TO_TICKS(1000)); // 1s 一点
    }

  
}



/* ===== GPIO 定义（低有效） ===== */
#define GPIO_CHARGE       GPIO_NUM_15   // 充电中
#define GPIO_CHARGE_FULL  GPIO_NUM_16   // 充满

typedef enum {
    CHARGE_STATE_NONE = 0,
    CHARGE_STATE_CHARGING,
    CHARGE_STATE_FULL,
} charge_state_t;

/* ===== LVGL 安全回调 ===== */
typedef struct {
    charge_state_t state;
    bool led_on;
} charge_ui_param_t;

static void charge_led_async_cb(void *user_data)
{
    charge_state_t state = (charge_state_t)user_data;

    if (!objects.charge_led) return;

    static bool blink_visible = false;

    switch (state) {

    case CHARGE_STATE_CHARGING:
        blink_visible = !blink_visible;

        if (blink_visible) {
            lv_obj_set_style_bg_color(
                objects.charge_led,
                lv_palette_main(LV_PALETTE_RED),
                LV_PART_MAIN
            );
            lv_obj_clear_flag(objects.charge_led, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(objects.charge_led, LV_OBJ_FLAG_HIDDEN);
        }
        break;

    case CHARGE_STATE_FULL:
        lv_obj_set_style_bg_color(
            objects.charge_led,
            lv_palette_main(LV_PALETTE_GREEN),
            LV_PART_MAIN
        );
        lv_obj_clear_flag(objects.charge_led, LV_OBJ_FLAG_HIDDEN);
        break;

    case CHARGE_STATE_NONE:
    default:
        lv_obj_add_flag(objects.charge_led, LV_OBJ_FLAG_HIDDEN);
        break;
    }
}


void charge_state_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << GPIO_CHARGE) | (1ULL << GPIO_CHARGE_FULL),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,   // 低有效
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    gpio_config(&cfg);

    ESP_LOGI(TAG, "Charge GPIO init OK");
}

void charge_state_task(void *arg)
{
    charge_state_t last_state = CHARGE_STATE_NONE;

    while (1) {
        int charge_lvl = gpio_get_level(GPIO_CHARGE);
        int full_lvl   = gpio_get_level(GPIO_CHARGE_FULL);

        charge_state_t cur_state;

        if (full_lvl == 0) {
            cur_state = CHARGE_STATE_FULL;
        } else if (charge_lvl == 0) {
            cur_state = CHARGE_STATE_CHARGING;
        } else {
            cur_state = CHARGE_STATE_NONE;
        }

        /* 状态变化 or 充电中（需要闪烁）才更新 UI */
        if (cur_state != last_state || cur_state == CHARGE_STATE_CHARGING) {
            lv_async_call(
                charge_led_async_cb,
                (void *)cur_state
            );
            last_state = cur_state;
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // 500ms 闪烁节奏
    }
}
