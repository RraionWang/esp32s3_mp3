/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/* AHT20 driver - shared I2C bus version (ESP-IDF v5.x) */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "vars.h"
#include "aht20.h"

// static const char *TAG = "AHT20";
static const char *TAG = "AHT20";


/* ================= AHT20 配置 ================= */
#define AHT20_SENSOR_ADDR 0x38
#define AHT20_I2C_FREQ_HZ 100000
#define AHT20_TIMEOUT_MS 1000

/* ================= 设备句柄 ================= */
static i2c_master_dev_handle_t s_aht20_dev = NULL;

/* =========================================================
 * AHT20 写命令（触发测量）
 * ========================================================= */
static esp_err_t aht20_start_measure(void)
{
    uint8_t cmd[3] = {0xAC, 0x33, 0x00};

    return i2c_master_transmit(
        s_aht20_dev,
        cmd,
        sizeof(cmd),
        AHT20_TIMEOUT_MS);
}

/* =========================================================
 * AHT20 读取 6 字节数据
 * ========================================================= */
static esp_err_t aht20_read_data(float *temperature, float *humidity)
{
    uint8_t data[6] = {0};

    /* 等待测量完成（datasheet ≥75ms） */
    vTaskDelay(pdMS_TO_TICKS(80));

    esp_err_t ret = i2c_master_receive(
        s_aht20_dev,
        data,
        sizeof(data),
        AHT20_TIMEOUT_MS);
    if (ret != ESP_OK)
    {
        return ret;
    }

    /* bit7 = 1 表示忙 */
    if (data[0] & 0x80)
    {
        ESP_LOGW(TAG, "AHT20 busy");
        return ESP_ERR_TIMEOUT;
    }

    /* 解析 20bit 湿度 */
    uint32_t raw_humi =
        ((uint32_t)data[1] << 12) |
        ((uint32_t)data[2] << 4) |
        ((data[3] & 0xF0) >> 4);

    /* 解析 20bit 温度 */
    uint32_t raw_temp =
        ((uint32_t)(data[3] & 0x0F) << 16) |
        ((uint32_t)data[4] << 8) |
        data[5];

    *humidity = raw_humi * 100.0f / 1048576.0f;
    *temperature = raw_temp * 200.0f / 1048576.0f - 50.0f;

    return ESP_OK;
}

/* =========================================================
 * AHT20 初始化（复用已有 I2C bus）
 * ========================================================= */
esp_err_t aht20_init(i2c_master_bus_handle_t bus)
{
    if (bus == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AHT20_SENSOR_ADDR,
        .scl_speed_hz = AHT20_I2C_FREQ_HZ,
    };

    ESP_ERROR_CHECK(
        i2c_master_bus_add_device(bus, &dev_cfg, &s_aht20_dev)

    );

    ESP_LOGI(TAG, "✅ AHT20 初始化完成（共享 I2C 总线）");
    return ESP_OK;
}

/* =========================================================
 * AHT20 任务
 * ========================================================= */

char aht20_str[64];

static void aht20_task(void *arg)
{
    float temp = 0.0f;
    float humi = 0.0f;

    ESP_LOGI(TAG, "AHT20 task started");

    while (1)
    {
        if (aht20_start_measure() == ESP_OK)
        {
            if (aht20_read_data(&temp, &humi) == ESP_OK)
            {
                // ESP_LOGI(TAG,
                //          "Temperature: %.2f °C | Humidity: %.2f %%RH",
                //          temp, humi);

                snprintf(
                    aht20_str,
                    sizeof(aht20_str),
                    "Temp: %.2f °C \n Humi: %.2f %%RH",
                    temp,
                    humi);

                set_var_aht20_text(aht20_str);
            }
            else
            {
                ESP_LOGE(TAG, "Read data failed");
            }
        }
        else
        {
            ESP_LOGE(TAG, "Start measure failed");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* =========================================================
 * 启动任务接口（给 app_main 用）
 * ========================================================= */
void aht20_start_task(void)
{
    xTaskCreate(
        aht20_task,
        "aht20_task",
        4096,
        NULL,
        5,
        NULL);
}


