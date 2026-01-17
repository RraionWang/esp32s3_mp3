#include "record.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#define TAG "RECORDER"

/* ================== 参数 ================== */

#define MOUNT_POINT        "/sdcard"

#define REC_SAMPLE_RATE    16000
#define REC_BITS           16
#define REC_CHANNELS       1

#define READ_FRAMES        512
#define MIC_GAIN           8

#define MIC_BCLK_IO        38
#define MIC_WS_IO          40
#define MIC_DIN_IO         39
#define MIC_CHANNEL_INDEX  0   /* Left */

/* ================= WAV 头 ================= */

typedef struct {
    char     riff_id[4];
    uint32_t riff_size;
    char     wave_id[4];
    char     fmt_id[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data_id[4];
    uint32_t data_size;
} wav_header_t;

/* ================= 命令 ================= */

typedef struct {
    recorder_cmd_t cmd;
    char path[32];
} recorder_msg_t;

/* ================= 状态 ================= */

static i2s_chan_handle_t s_rx_chan = NULL;
static QueueHandle_t     s_cmd_queue = NULL;
static TaskHandle_t      s_task = NULL;

static bool   s_recording = false;
static FILE  *s_file = NULL;
static char   s_path[32];
static uint32_t s_data_bytes = 0;

/* ================================================= */

static bool find_next_record_file(char *out, size_t len)
{
    for (int i = 0; i <= 9999; i++) {
        snprintf(out, len, MOUNT_POINT "/REC%04d.WAV", i);
        FILE *f = fopen(out, "rb");
        if (!f) {
            return true;
        }
        fclose(f);
    }
    return false;
}

/* ================================================= */

static void record_task(void *arg)
{
    const size_t rx_bytes = READ_FRAMES * 2 * sizeof(int32_t);

    int32_t *rx_buf = heap_caps_malloc(
        rx_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

    int16_t *out_buf = heap_caps_malloc(
        READ_FRAMES * sizeof(int16_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!rx_buf || !out_buf) {
        ESP_LOGE(TAG, "malloc failed");
        vTaskDelete(NULL);
        return;
    }

    recorder_msg_t msg;

    while (1) {

        /* 等待命令（非录音时会阻塞在这里） */
        if (!s_recording) {
            if (xQueueReceive(s_cmd_queue, &msg, portMAX_DELAY) == pdTRUE) {
                if (msg.cmd == REC_CMD_START) {
                    strncpy(s_path, msg.path, sizeof(s_path) - 1);
                    s_path[sizeof(s_path) - 1] = 0;

                    s_file = fopen(s_path, "wb");
                    if (!s_file) {
                        ESP_LOGE(TAG, "Open failed: %s", s_path);
                        continue;
                    }

                    wav_header_t header = {
                        .riff_id = {'R','I','F','F'},
                        .riff_size = 36,
                        .wave_id = {'W','A','V','E'},
                        .fmt_id  = {'f','m','t',' '},
                        .fmt_size = 16,
                        .audio_format = 1,
                        .num_channels = REC_CHANNELS,
                        .sample_rate = REC_SAMPLE_RATE,
                        .byte_rate = REC_SAMPLE_RATE * REC_CHANNELS * 2,
                        .block_align = REC_CHANNELS * 2,
                        .bits_per_sample = 16,
                        .data_id = {'d','a','t','a'},
                        .data_size = 0,
                    };

                    fwrite(&header, 1, sizeof(header), s_file);
                    s_data_bytes = 0;
                    s_recording = true;

                    ESP_LOGI(TAG, "Recording START: %s", s_path);
                }
            }
            continue;
        }

        /* ===== 正在录音 ===== */

        size_t bytes_read = 0;

        if (i2s_channel_read(
                s_rx_chan,
                rx_buf,
                rx_bytes,
                &bytes_read,
                100) == ESP_OK && bytes_read) {

            int frames = bytes_read / (sizeof(int32_t) * 2);

            for (int i = 0; i < frames; i++) {
                int32_t sample = rx_buf[i * 2 + MIC_CHANNEL_INDEX];

                sample *= MIC_GAIN;
                if (sample > INT32_MAX) sample = INT32_MAX;
                if (sample < INT32_MIN) sample = INT32_MIN;

                out_buf[i] = (int16_t)(sample >> 16);
            }

            fwrite(out_buf, sizeof(int16_t), frames, s_file);
            s_data_bytes += frames * sizeof(int16_t);
        }

        /* 非阻塞检查 STOP 命令 */
        if (xQueueReceive(s_cmd_queue, &msg, 0) == pdTRUE) {
            if (msg.cmd == REC_CMD_STOP) {

                wav_header_t header;
                fseek(s_file, 0, SEEK_SET);
                fread(&header, 1, sizeof(header), s_file);

                header.data_size = s_data_bytes;
                header.riff_size = 36 + s_data_bytes;

                fseek(s_file, 0, SEEK_SET);
                fwrite(&header, 1, sizeof(header), s_file);

                fclose(s_file);
                s_file = NULL;
                s_recording = false;

                ESP_LOGI(TAG, "Recording STOP: %s (%lu bytes)",
                         s_path, (unsigned long)s_data_bytes);
            }
        }
    }
}

/* ================================================= */

esp_err_t recorder_init(void)
{
    if (s_rx_chan) return ESP_OK;

    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &s_rx_chan));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(REC_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_32BIT,
                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = MIC_BCLK_IO,
            .ws   = MIC_WS_IO,
            .din  = MIC_DIN_IO,
            .dout = I2S_GPIO_UNUSED,
        },
    };

    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx_chan));

    s_cmd_queue = xQueueCreate(4, sizeof(recorder_msg_t));
    if (!s_cmd_queue) return ESP_FAIL;

    xTaskCreate(
        record_task,
        "record_task",
        4096,
        NULL,
        5,
        &s_task
    );

    ESP_LOGI(TAG, "Recorder initialized");
    return ESP_OK;
}

/* ================================================= */

esp_err_t recorder_start(void)
{
    if (s_recording) return ESP_ERR_INVALID_STATE;

    recorder_msg_t msg = {0};
    msg.cmd = REC_CMD_START;

    if (!find_next_record_file(msg.path, sizeof(msg.path))) {
        ESP_LOGE(TAG, "No free RECxxxx slot");
        return ESP_FAIL;
    }

    xQueueSend(s_cmd_queue, &msg, portMAX_DELAY);
    return ESP_OK;
}

esp_err_t recorder_stop(void)
{
    if (!s_recording) return ESP_ERR_INVALID_STATE;

    recorder_msg_t msg = {
        .cmd = REC_CMD_STOP
    };

    xQueueSend(s_cmd_queue, &msg, portMAX_DELAY);
    return ESP_OK;
}

bool recorder_is_recording(void)
{
    return s_recording;
}

const char *recorder_current_path(void)
{
    return s_recording ? s_path : NULL;
}
