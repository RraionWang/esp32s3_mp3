
#include "lcd.h"
#include "pin_cfg.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "ui.h" // 引入 UI 头文件
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* LCD size */
#define LCD_H_RES (172)
#define LCD_V_RES (320)

/* LCD settings */
#define LCD_SPI_NUM (SPI3_HOST)
#define LCD_PIXEL_CLK_HZ (20 * 1000 * 1000)
#define LCD_CMD_BITS (8)
#define LCD_PARAM_BITS (8)
#define LCD_BITS_PER_PIXEL (16)
#define LCD_DRAW_BUFF_DOUBLE (1)
#define LCD_DRAW_BUFF_HEIGHT (50)
#define LCD_BL_ON_LEVEL (1)

static const char *TAG = "LVGL";

static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_panel_handle_t lcd_panel = NULL;
static lv_display_t *lvgl_disp = NULL;

/* 初始化 LCD */
esp_err_t app_lcd_init(void)
{
    esp_err_t ret = ESP_OK;

    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_GPIO_BL};
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    const spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_GPIO_SCLK,
        .mosi_io_num = LCD_GPIO_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_H_RES * LCD_DRAW_BUFF_HEIGHT * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_GPIO_DC,
        .cs_gpio_num = LCD_GPIO_CS,
        .pclk_hz = LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_NUM, &io_config, &lcd_io),
                      err, TAG, "New panel IO failed");

    // ✅ 正确顺序：先创建 panel，再初始化
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_GPIO_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = LCD_BITS_PER_PIXEL,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(lcd_io, &panel_config, &lcd_panel),
                      err, TAG, "New panel failed");

    // ✅ 然后再 reset + init
    esp_lcd_panel_reset(lcd_panel);
    esp_lcd_panel_init(lcd_panel);

    // ✅ 显示方向修正
    esp_lcd_panel_mirror(lcd_panel, true, true);
    esp_lcd_panel_set_gap(lcd_panel, 0, 34);

    // // ✅ 关闭反色
    // esp_lcd_panel_invert_color(lcd_panel, true);

    // ✅ 打开背光
    gpio_set_level(LCD_GPIO_BL, LCD_BL_ON_LEVEL);
    esp_lcd_panel_disp_on_off(lcd_panel, true);

    return ESP_OK;

err:
    if (lcd_panel)
        esp_lcd_panel_del(lcd_panel);
    if (lcd_io)
        esp_lcd_panel_io_del(lcd_io);
    spi_bus_free(LCD_SPI_NUM);
    return ret;
}

/* 初始化 LVGL */
esp_err_t app_lvgl_init(void)
{
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,
        .task_stack = 8192,
        .task_affinity = -1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5};
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL init failed");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io,
        .panel_handle = lcd_panel,
        .buffer_size = LCD_H_RES * LCD_DRAW_BUFF_HEIGHT,
        .double_buffer = LCD_DRAW_BUFF_DOUBLE,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .rotation = {.swap_xy = false, .mirror_x = false, .mirror_y = true},
        .flags = {.buff_dma = true, .swap_bytes = true}};
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    lv_disp_set_rotation(lvgl_disp, LV_DISPLAY_ROTATION_90);

    ctp_register_lvgl(lvgl_disp);

    return ESP_OK;
}

// 以下是触摸驱动

/* ========== 配置参数 ========== */
#define CTP_I2C_FREQ_HZ 400000
#define CTP_I2C_ADDR 0x15

#define TP_H_RES 172
#define TP_V_RES 320

#define TP_SWAP_XY 0
#define TP_MIRROR_X 1
#define TP_MIRROR_Y 1
#define TP_GAP_X 0
#define TP_GAP_Y 34

 i2c_master_bus_handle_t s_bus = NULL;
 i2c_master_dev_handle_t s_dev = NULL;
 
static lv_indev_t *s_indev = NULL;

/* ========== 基础I2C操作函数 ========== */
static esp_err_t ctp_reg_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, 1000);
}

static esp_err_t ctp_reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 1000);
}

/* ========== 触摸复位 ========== */
static void ctp_reset(void)
{
    gpio_config_t rst = {
        .pin_bit_mask = (1ULL << CTP_PIN_RST),
        .mode = GPIO_MODE_OUTPUT};
    gpio_config(&rst);
    gpio_set_level(CTP_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(CTP_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

/* ========== 坐标映射 ========== */
static void tp_map_to_lvgl(uint16_t rx, uint16_t ry, lv_point_t *pt)
{
    int16_t x = rx, y = ry;
    x -= TP_GAP_X;
    if (x < 0)
        x = 0;
    y -= TP_GAP_Y;
    if (y < 0)
        y = 0;

#if TP_SWAP_XY
    int16_t t = x;
    x = y;
    y = t;
#endif
#if TP_MIRROR_X
    x = (TP_H_RES - 1) - x;
#endif
#if TP_MIRROR_Y
    y = (TP_V_RES - 1) - y;
#endif
    if (x >= TP_H_RES)
        x = TP_H_RES - 1;
    if (y >= TP_V_RES)
        y = TP_V_RES - 1;

    pt->x = x;
    pt->y = y;
}

/* ========== 读取触摸坐标 ========== */
static bool ctp_read_once(lv_point_t *pt)
{
    uint8_t buf[7] = {0};
    if (ctp_reg_read(0x01, buf, sizeof(buf)) != ESP_OK)
        return false;

    uint8_t points = buf[1] & 0x0F;
    if (points == 0)
        return false;

    uint16_t x = ((buf[2] & 0x0F) << 8) | buf[3];
    uint16_t y = ((buf[4] & 0x0F) << 8) | buf[5];

    tp_map_to_lvgl(x, y, pt);

    ESP_LOGI("LVGL", "x=%d,y=%d", x, y);
    return true;
}

/* ========== LVGL回调 ========== */
static void ctp_lvgl_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    lv_point_t p;
    if (ctp_read_once(&p))
    {
        data->point = p;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* ========== 公共接口实现 ========== */
esp_err_t ctp_init(void)
{
    // 1. 初始化I2C总线
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .scl_io_num = CTP_PIN_SCL,
        .sda_io_num = CTP_PIN_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_bus), TAG, "i2c bus create failed");

    // 2. 绑定CST816D设备
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CTP_I2C_ADDR,
        .scl_speed_hz = CTP_I2C_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev), TAG, "add device failed");

    // 3. 复位触摸芯片
    ctp_reset();

    // 4. 读取芯片ID（可选）
    uint8_t id = 0;
    if (ctp_reg_read(0xA7, &id, 1) == ESP_OK)
        ESP_LOGI(TAG, "CST816D ID: 0x%02X", id);

    ESP_LOGI(TAG, "✅ CST816D 初始化完成");
    return ESP_OK;
}

lv_indev_t *ctp_register_lvgl(lv_display_t *disp)
{
    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, ctp_lvgl_read_cb);
    lv_indev_set_display(s_indev, disp);
    ESP_LOGI(TAG, "LVGL 输入设备注册完成");
    return s_indev;
}
