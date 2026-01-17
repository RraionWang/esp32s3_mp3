#include <stdio.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "esp_heap_caps.h"

static const char *TAG = "I2S_DEBUG";

// Pin definitions
#define EXAMPLE_STD_BCLK_IO1        38
#define EXAMPLE_STD_WS_IO1          40
#define EXAMPLE_STD_DIN_IO1         39



#define SAMPLE_RATE     (16000)
#define READ_LEN        (512)  // frames per read (stereo frames)
#define MIC_CHANNEL_LEFT  0
#define MIC_CHANNEL_RIGHT 1
#define MIC_CHANNEL_INDEX MIC_CHANNEL_LEFT
#define RECORD_SECONDS  (10)
#define SD_USE_SPI      (1)
#define MOUNT_POINT     "/sdcard"
#define RECORD_FILE_PATH MOUNT_POINT "/REC0001.WAV"


#define MIC_GAIN  8   // 2, 4, 8 都可以试

    
typedef struct {
    char riff_id[4];      // "RIFF"
    uint32_t riff_size;   // 36 + data_size
    char wave_id[4];      // "WAVE"
    char fmt_id[4];       // "fmt "
    uint32_t fmt_size;    // 16
    uint16_t audio_format; // 1 = PCM
    uint16_t num_channels; // 1 = mono
    uint32_t sample_rate;  // SAMPLE_RATE
    uint32_t byte_rate;    // sample_rate * num_channels * bits_per_sample / 8
    uint16_t block_align;  // num_channels * bits_per_sample / 8
    uint16_t bits_per_sample; // 16
    char data_id[4];       // "data"
    uint32_t data_size;    // data bytes
} wav_header_t;

static i2s_chan_handle_t rx_chan;
static sdmmc_card_t *sdcard;

void init_mic_debug(void)
{
    // Channel config
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

    // Standard mode config
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = EXAMPLE_STD_BCLK_IO1,
            .ws   = EXAMPLE_STD_WS_IO1,
            .din  = EXAMPLE_STD_DIN_IO1,
            .dout = I2S_GPIO_UNUSED,
        },
    };
    
    // Many digital mics transmit in 32-bit slots
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    ESP_LOGI(TAG, "I2S Mic initialized on BCLK:%d, WS:%d, DIN:%d", 
             EXAMPLE_STD_BCLK_IO1, EXAMPLE_STD_WS_IO1, EXAMPLE_STD_DIN_IO1);
}


void record_to_sd_task(void *args)
{
    const size_t bytes_per_sample = sizeof(int16_t);
    const size_t buffer_size = READ_LEN * 2 * sizeof(int32_t);
    int32_t *rx_buf = (int32_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    int16_t *out_buf = (int16_t *)malloc(READ_LEN * sizeof(int16_t));

    if (rx_buf == NULL || out_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffers");
        free(out_buf);
        free(rx_buf);
        vTaskDelete(NULL);
        return;
    }

    FILE *f = fopen(RECORD_FILE_PATH, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", strerror(errno));
        free(out_buf);
        free(rx_buf);
        vTaskDelete(NULL);
        return;
    }

    wav_header_t header = {
        .riff_id = { 'R', 'I', 'F', 'F' },
        .riff_size = 36,
        .wave_id = { 'W', 'A', 'V', 'E' },
        .fmt_id = { 'f', 'm', 't', ' ' },
        .fmt_size = 16,
        .audio_format = 1,
        .num_channels = 1,
        .sample_rate = SAMPLE_RATE,
        .byte_rate = SAMPLE_RATE * 2,
        .block_align = 2,
        .bits_per_sample = 16,
        .data_id = { 'd', 'a', 't', 'a' },
        .data_size = 0,
    };

    if (fwrite(&header, 1, sizeof(header), f) != sizeof(header)) {
        ESP_LOGE(TAG, "Failed to write WAV header");
        fclose(f);
        free(out_buf);
        free(rx_buf);
        vTaskDelete(NULL);
        return;
    }

    size_t bytes_read = 0;
    size_t bytes_written_total = 0;
    const size_t bytes_target = SAMPLE_RATE * bytes_per_sample * RECORD_SECONDS;

    ESP_LOGI(TAG, "Recording %d seconds to %s", RECORD_SECONDS, RECORD_FILE_PATH);
    while (bytes_written_total < bytes_target) {
        if (i2s_channel_read(rx_chan, rx_buf, buffer_size, &bytes_read, 1000) == ESP_OK) {
            if (bytes_read > 0) {
                size_t frames = bytes_read / (sizeof(int32_t) * 2);
                for (size_t i = 0; i < frames; ++i) {
                    // int32_t sample = rx_buf[i * 2 + MIC_CHANNEL_INDEX];
                    // out_buf[i] = (int16_t)(sample >> 16);

                    int32_t sample = rx_buf[i * 2 + MIC_CHANNEL_INDEX];

/* 软件放大 */
sample = sample * MIC_GAIN;

/* 防止溢出（非常重要） */
if (sample > INT32_MAX) sample = INT32_MAX;
if (sample < INT32_MIN) sample = INT32_MIN;

/* 转成 16bit */
out_buf[i] = (int16_t)(sample >> 16);

                }
                size_t to_write = frames * bytes_per_sample;
                if (bytes_written_total + to_write > bytes_target) {
                    to_write = bytes_target - bytes_written_total;
                }
                size_t wrote = fwrite(out_buf, 1, to_write, f);
                if (wrote != to_write) {
                    ESP_LOGE(TAG, "Write error");
                    break;
                }
                bytes_written_total += wrote;
            }
        } else {
            ESP_LOGW(TAG, "Read Failed");
        }
    }

    header.data_size = bytes_written_total;
    header.riff_size = 36 + header.data_size;
    fseek(f, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(header), f);
    fclose(f);
    free(out_buf);
    free(rx_buf);
    ESP_LOGI(TAG, "Recording finished, bytes: %u", (unsigned)bytes_written_total);
    vTaskDelete(NULL);
}

// // Called from app_main
// void app_main(void)
// {
//     init_mic_debug();
    
//     xTaskCreate(record_to_sd_task, "record_to_sd_task", 8192, NULL, 5, NULL);
// }
