// wav_player.c
#include "audio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"

#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "vars.h"
#include "screens.h"
#include "esp_heap_caps.h"

static uint32_t s_wav_sample_rate = 0;
static uint16_t s_wav_bits = 0;
static uint16_t s_wav_channels = 0;

static const char *TAG = "wav_player";

/* ===== I2S 配置（可根据你的硬件修改）===== */

#define SPK_CTRL_IO 4
#define SPK_WS_IO 5
#define SPK_BCLK_IO 6
#define SPK_DATA_IO 7

#define SAMPLE_RATE 44100
#define READ_SAMPLES 512
#define READ_BYTES (READ_SAMPLES * sizeof(int16_t)) // 16-bit mono
static long s_current_file_offset = 44;             // 初始为 WAV 数据起始位置（跳过 44 字节头）

/* ===== 内部状态 ===== */
typedef enum
{
    PLAYER_STATE_IDLE,
    PLAYER_STATE_PLAYING,
    PLAYER_STATE_PAUSED,
    PLAYER_STATE_STOPPED,

} player_state_t;

typedef struct
{
    wav_player_cmd_t cmd;
    float volume; // 仅当 cmd == WAV_CMD_SET_VOLUME 时有效
    int index;    //
} player_cmd_msg_t;

static player_state_t s_state = PLAYER_STATE_IDLE;
static float s_volume = 0.9f;
static i2s_chan_handle_t s_tx_chan = NULL;
static SemaphoreHandle_t s_sd_mutex = NULL; // 这个互斥锁用来保护对sd卡的访问
static QueueHandle_t s_cmd_queue = NULL;
static QueueHandle_t s_ui_queue = NULL; // 这个队列用来发送指令
static const char **s_playlist = NULL;
static int s_playlist_len = 0;
static int s_current_index = 0;
static FILE *s_current_file = NULL;
static bool s_resume_open = false;
static bool s_switching_track = false;

static uint32_t s_wav_data_bytes = 0; // 当前歌曲 PCM 总字节数
static uint32_t s_bytes_per_sec = 0;  // 每秒 PCM 字节数
static uint32_t s_played_bytes = 0;   // 已播放 PCM 字节数

// 歌词相关数据

#define MAX_LRC_LINES 128
#define MAX_LRC_TEXT 64

typedef struct
{
    uint32_t time_ms;
    char text[MAX_LRC_TEXT];
} lrc_line_t;

// static lrc_line_t s_lrc_lines[MAX_LRC_LINES];

// 修改定义，申请内存到psram
static lrc_line_t *s_lrc_lines = NULL;

static int s_lrc_count = 0;
static int s_lrc_index = -1;

static void ui_queue_send(const wav_player_ui_msg_t *msg)
{
    if (!s_ui_queue || !msg)
    {
        return;
    }
    (void)xQueueSend(s_ui_queue, msg, 0);
}

static uint32_t lrc_parse_time_ms(const char *tag)
{
    int mm = 0, ss = 0, xx = 0;
    sscanf(tag, "[%d:%d.%d]", &mm, &ss, &xx);
    return mm * 60000 + ss * 1000 + xx * 10;
}

static bool lrc_load_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        s_lrc_count = 0;
        s_lrc_index = -1;
        // ESP_LOGE(TAG, "Failed to open LRC: %s", path);
        return false;
    }

    char line[128];
    s_lrc_count = 0;
    s_lrc_index = -1;

    while (fgets(line, sizeof(line), fp))
    {
        if (line[0] != '[')
            continue;

        char *end = strchr(line, ']');
        if (!end)
            continue;

        s_lrc_lines[s_lrc_count].time_ms =
            lrc_parse_time_ms(line);

        strncpy(
            s_lrc_lines[s_lrc_count].text,
            end + 1,
            MAX_LRC_TEXT - 1);

        // 去掉结尾换行
        char *nl = strchr(s_lrc_lines[s_lrc_count].text, '\n');
        if (nl)
            *nl = '\0';

        s_lrc_count++;
        if (s_lrc_count >= MAX_LRC_LINES)
            break;
    }

    fclose(fp);

    // ESP_LOGI(TAG, "Loaded %d LRC lines", s_lrc_count);
    return s_lrc_count > 0;
}

static void lrc_update_by_time(float current_time_sec)
{
    if (s_lrc_count == 0)
    {
        return;
    }
    uint32_t current_ms = (uint32_t)(current_time_sec * 1000);

    for (int i = s_lrc_index + 1; i < s_lrc_count; i++)
    {
        if (current_ms < s_lrc_lines[i].time_ms)
        {
            break;
        }

        s_lrc_index = i;

        // 推送到屏幕（你指定的接口）
        {
            wav_player_ui_msg_t msg = {0};
            msg.type = WAV_UI_UPDATE_LRC;
            strncpy(msg.lrc, s_lrc_lines[i].text, sizeof(msg.lrc) - 1);
            msg.lrc[sizeof(msg.lrc) - 1] = '\0';
            ui_queue_send(&msg);
        }

        ESP_LOGD(TAG, "LRC: %s", s_lrc_lines[i].text);
    }
}

static void lrc_seek_by_time(float current_time_sec, bool emit)
{
    uint32_t current_ms = (uint32_t)(current_time_sec * 1000);
    int idx = -1;

    for (int i = 0; i < s_lrc_count; i++)
    {
        if (current_ms < s_lrc_lines[i].time_ms)
        {
            break;
        }
        idx = i;
    }

    s_lrc_index = idx;
    if (emit && idx >= 0)
    {
        wav_player_ui_msg_t msg = {0};
        msg.type = WAV_UI_UPDATE_LRC;
        strncpy(msg.lrc, s_lrc_lines[idx].text, sizeof(msg.lrc) - 1);
        msg.lrc[sizeof(msg.lrc) - 1] = '\0';
        ui_queue_send(&msg);
    }
}

static bool read_wav_header(FILE *fp)
{
    uint8_t buf[8];

    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    uint16_t channels = 0;
    uint32_t data_size = 0;

    fseek(fp, 12, SEEK_SET); // 跳过 RIFF + size + WAVE

    while (1)
    {
        if (fread(buf, 1, 8, fp) != 8)
            return false;

        uint32_t chunk_size = *(uint32_t *)&buf[4];

        if (memcmp(buf, "fmt ", 4) == 0)
        {
            uint8_t fmt[16];
            fread(fmt, 1, 16, fp);

            channels = *(uint16_t *)&fmt[2];
            sample_rate = *(uint32_t *)&fmt[4];
            bits_per_sample = *(uint16_t *)&fmt[14];

            s_wav_sample_rate = sample_rate;
            s_wav_bits = bits_per_sample;
            s_wav_channels = channels;

            fseek(fp, chunk_size - 16, SEEK_CUR);
        }
        else if (memcmp(buf, "data", 4) == 0)
        {
            data_size = chunk_size;
            break;
        }
        else
        {
            // 跳过未知 chunk
            fseek(fp, chunk_size, SEEK_CUR);
        }
    }

    if (sample_rate == 0 || bits_per_sample == 0 || data_size == 0)
        return false;

    s_wav_data_bytes = data_size;
    s_bytes_per_sec = sample_rate * channels * (bits_per_sample / 8);
    s_played_bytes = 0;

    // ESP_LOGI(TAG,
    //          "WAV OK: %lu Hz, %u ch, %u bit, data=%lu bytes, %lu B/s",
    //          sample_rate, channels, bits_per_sample,
    //          (unsigned long)data_size,
    //          (unsigned long)s_bytes_per_sec);


             if (bits_per_sample != 8 &&
    bits_per_sample != 16 &&
    bits_per_sample != 24) {
    ESP_LOGE(TAG, "Unsupported WAV bit depth");
    return false;
}



    return true;
}

static esp_err_t i2s_reconfig_from_wav(void)
{
    ESP_ERROR_CHECK(i2s_channel_disable(s_tx_chan));

    i2s_std_clk_config_t clk_cfg =
        I2S_STD_CLK_DEFAULT_CONFIG(s_wav_sample_rate);

    ESP_ERROR_CHECK(
        i2s_channel_reconfig_std_clock(s_tx_chan, &clk_cfg)
    );

    ESP_ERROR_CHECK(i2s_channel_enable(s_tx_chan));

    // ESP_LOGI(TAG,
    //     "I2S clock reconfig OK: %lu Hz",
    //     (unsigned long)s_wav_sample_rate
    // );

    return ESP_OK;
}


/* ===== I2S 初始化 ===== */
static esp_err_t i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx_chan, NULL));

    i2s_std_slot_config_t slot_cfg = {
        .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
        .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
        .slot_mode = I2S_SLOT_MODE_STEREO,
        .slot_mask = I2S_STD_SLOT_BOTH,
        .ws_width = 16,
        .ws_pol = false,
        .bit_shift = false,
        .left_align = false,
        .big_endian = false,
        .bit_order_lsb = false};

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = slot_cfg,
        .gpio_cfg = {
            .bclk = SPK_BCLK_IO,
            .ws = SPK_WS_IO,
            .dout = SPK_DATA_IO,
            .din = I2S_GPIO_UNUSED}};

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx_chan));

    gpio_set_direction(SPK_CTRL_IO, GPIO_MODE_OUTPUT);
    gpio_set_level(SPK_CTRL_IO, 1); // 开启功放
    return ESP_OK;
}

/* ===== 安全关闭当前文件 ===== */
static void safe_close_file(void)
{
    if (s_current_file)
    {
        if (xSemaphoreTake(s_sd_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            s_current_file_offset = ftell(s_current_file); // 保存当前偏移
            fclose(s_current_file);
            s_current_file = NULL;
            xSemaphoreGive(s_sd_mutex);
        }
    }
}

/* ===== 打开当前歌曲 ===== */

static bool open_current_track(void)
{
    safe_close_file(); // ensure old file handle is closed
    bool switching = s_switching_track;
    s_switching_track = false;

    const char *path = s_playlist[s_current_index];
    if (xSemaphoreTake(s_sd_mutex, pdMS_TO_TICKS(2000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to lock SD for open");
        return false;
    }

    s_current_file = fopen(path, "rb");

    if (!s_current_file)
    {
        ESP_LOGE(TAG, "Failed to open %s", path);
        xSemaphoreGive(s_sd_mutex);
        return false;
    }

    if (!read_wav_header(s_current_file))
    {
    

        ESP_LOGE(TAG, "Invalid WAV header");
        fclose(s_current_file);
        s_current_file = NULL;
        xSemaphoreGive(s_sd_mutex);
        return false;
    }

        i2s_reconfig_from_wav();

    char lrc_path[128];
    strcpy(lrc_path, path);
    char *dot = strrchr(lrc_path, '.');
    if (dot)
        strcpy(dot, ".lrc");

    // ESP_LOGI("LRC", "文件名字是%s", lrc_path);

    if (!lrc_load_file(lrc_path))
    {
        wav_player_ui_msg_t msg = {0};
        msg.type = WAV_UI_UPDATE_LRC;
        strncpy(msg.lrc, "no lrc file", sizeof(msg.lrc) - 1);
        msg.lrc[sizeof(msg.lrc) - 1] = '\0';
        ui_queue_send(&msg);
    }

    // 跳转到上次保存的播放位置（必须 >= 44）
    if (switching)
    {
        s_current_file_offset = 44;
        s_played_bytes = 0;
    }

    if (s_current_file_offset < 44)
    {
        s_current_file_offset = 44; // 安全兜底
    }

    if (fseek(s_current_file, s_current_file_offset, SEEK_SET) != 0)
    {
        ESP_LOGE(TAG, "fseek to %ld failed", s_current_file_offset);
        fclose(s_current_file);
        s_current_file = NULL;
        xSemaphoreGive(s_sd_mutex);
        return false;
    }

    if (s_resume_open)
    {
        if (s_current_file_offset >= 44)
        {
            s_played_bytes = (uint32_t)(s_current_file_offset - 44);
        }
        float resume_time = (s_bytes_per_sec > 0)
                                ? ((float)s_played_bytes / s_bytes_per_sec)
                                : 0.0f;
        lrc_seek_by_time(resume_time, true);
    }

    xSemaphoreGive(s_sd_mutex);
    // ESP_LOGI(TAG, "Now playing: %s at offset %ld", path, s_current_file_offset);
    return true;
}

static char s_cur_time_str[8]; // "MM:SS\0"
static char s_total_time_str[8];

static void format_time_mmss(float seconds, char *out, size_t out_len)
{
    int sec = (int)seconds;
    int mm = sec / 60;
    int ss = sec % 60;

    snprintf(out, out_len, "%02d:%02d", mm, ss);
}

static void ui_send_time_reset(void)
{
    float total_time = (s_bytes_per_sec > 0)
                           ? ((float)s_wav_data_bytes / s_bytes_per_sec)
                           : 0.0f;

    format_time_mmss(0.0f, s_cur_time_str, sizeof(s_cur_time_str));
    format_time_mmss(total_time, s_total_time_str, sizeof(s_total_time_str));

    wav_player_ui_msg_t msg = {0};
    msg.type = WAV_UI_UPDATE_TIME;
    strncpy(msg.current_time, s_cur_time_str, sizeof(msg.current_time) - 1);
    strncpy(msg.total_time, s_total_time_str, sizeof(msg.total_time) - 1);
    msg.current_time[sizeof(msg.current_time) - 1] = '\0';
    msg.total_time[sizeof(msg.total_time) - 1] = '\0';
    msg.percent = 0;
    ui_queue_send(&msg);
}

/* ===== 播放任务 ===== */
static void wav_play_task(void *arg)
{
    int16_t *file_buf = heap_caps_malloc(READ_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int16_t *i2s_buf = heap_caps_malloc(READ_SAMPLES * 2 * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!file_buf || !i2s_buf)
    {
        ESP_LOGE(TAG, "malloc failed");
        vTaskDelete(NULL);
        return;
    }

    // 初始打开第一首
    if (!open_current_track())
    {
        s_state = PLAYER_STATE_STOPPED;
    }
    else
    {
        s_state = PLAYER_STATE_PLAYING;
    }

    while (1)
    {

        // 对不同指令的反应
        player_cmd_msg_t msg;
        // 非阻塞接收命令（1ms 超时）
        if (xQueueReceive(s_cmd_queue, &msg, pdMS_TO_TICKS(1)) == pdTRUE)
        {
            switch (msg.cmd)
            {
            case WAV_CMD_PAUSE:
                if (s_state == PLAYER_STATE_PLAYING)
                {

                    gpio_set_level(SPK_CTRL_IO, 0); // 关闭功放
                    safe_close_file();              // ensure old file handle is closed
                    s_state = PLAYER_STATE_PAUSED;
                    ESP_LOGI(TAG, "Paused");
                }
                break;

            case WAV_CMD_RESUME:

                if (s_state == PLAYER_STATE_PAUSED)
                {
                    gpio_set_level(SPK_CTRL_IO, 1); // 开启功放
                    s_resume_open = true;
                    if (open_current_track())
                    {
                        s_state = PLAYER_STATE_PLAYING;
                    }
                    s_resume_open = false;
                }
                break;

            case WAV_CMD_STOP:
                gpio_set_level(SPK_CTRL_IO, 0); // 关闭功放
                safe_close_file();              // ensure old file handle is closed
                s_current_file_offset = 44;
                s_state = PLAYER_STATE_STOPPED;

                s_lrc_index = -1;
                {
                    wav_player_ui_msg_t msg = {0};
                    msg.type = WAV_UI_CLEAR_LRC;
                    ui_queue_send(&msg);
                }

                ESP_LOGI(TAG, "Stopped");

                break;

            case WAV_CMD_NEXT:
                s_current_index = (s_current_index + 1) % s_playlist_len;
                s_switching_track = true;
                if (open_current_track())
                {
                    s_state = PLAYER_STATE_PLAYING;
                    ui_send_time_reset();
                }
                s_lrc_index = -1;
                {
                    wav_player_ui_msg_t msg = {0};
                    msg.type = WAV_UI_CLEAR_LRC;
                    ui_queue_send(&msg);
                }

                break;

            case WAV_CMD_PREV:
                s_current_index = (s_current_index - 1 + s_playlist_len) % s_playlist_len;
                s_switching_track = true;
                if (open_current_track())
                {
                    s_state = PLAYER_STATE_PLAYING;
                    ui_send_time_reset();
                }
                s_lrc_index = -1;
                {
                    wav_player_ui_msg_t msg = {0};
                    msg.type = WAV_UI_CLEAR_LRC;
                    ui_queue_send(&msg);
                }

                break;

            case WAV_CMD_PLAY_INDEX:
                if (msg.index < 0 || msg.index >= s_playlist_len)
                {
                    ESP_LOGE(TAG, "Invalid index %d", msg.index);
                    break;
                }
                s_current_index = msg.index;
                s_switching_track = true;
                if (open_current_track())
                {
                    s_state = PLAYER_STATE_PLAYING;
                    ui_send_time_reset();
                }
                s_lrc_index = -1;
                {
                    wav_player_ui_msg_t msg = {0};
                    msg.type = WAV_UI_CLEAR_LRC;
                    ui_queue_send(&msg);
                }

                break;

            case WAV_CMD_SET_VOLUME:
                wav_player_set_volume(msg.volume);
                ESP_LOGI(TAG, "Volume set to %.2f", msg.volume);
                break;

            default:
                break;
            }
        }

        if (s_state != PLAYER_STATE_PLAYING)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // 读取音频数据
        if (xSemaphoreTake(s_sd_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        {
            ESP_LOGE(TAG, "SD mutex timeout on read");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        size_t r = fread(file_buf, 1, READ_BYTES, s_current_file);

        if (r > 0)
        {
            s_played_bytes += r;
            s_current_file_offset += r;
        }

        // 打印播放时间和百分比
        float current_time = (float)s_played_bytes / s_bytes_per_sec;

        float total_time = (float)s_wav_data_bytes / s_bytes_per_sec;
        float percent = (total_time > 0)
                            ? (current_time / total_time * 100.0f)
                            : 0.0f;
        static uint32_t last_print_ms = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (now - last_print_ms >= 500)
        {
            last_print_ms = now;

            ESP_LOGI(TAG,
                     "Time: %02d:%02d / %02d:%02d (%.1f%%)",
                     (int)(current_time / 60),
                     (int)((int)current_time % 60),
                     (int)(total_time / 60),
                     (int)((int)total_time % 60),
                     percent);

            format_time_mmss(current_time, s_cur_time_str, sizeof(s_cur_time_str));
            format_time_mmss(total_time, s_total_time_str, sizeof(s_total_time_str));

            {
                wav_player_ui_msg_t msg = {0};
                msg.type = WAV_UI_UPDATE_TIME;
                strncpy(msg.current_time, s_cur_time_str, sizeof(msg.current_time) - 1);
                strncpy(msg.total_time, s_total_time_str, sizeof(msg.total_time) - 1);
                msg.current_time[sizeof(msg.current_time) - 1] = '\0';
                msg.total_time[sizeof(msg.total_time) - 1] = '\0';
                msg.percent = (uint8_t)percent;
                ui_queue_send(&msg);
            }

            lrc_update_by_time(current_time);
        }

        xSemaphoreGive(s_sd_mutex);

        if (r == 0)
        {
            // 当前曲目结束，自动下一首
            s_current_file_offset = 44;
            s_current_index = (s_current_index + 1) % s_playlist_len;
            if (!open_current_track())
            {
                s_state = PLAYER_STATE_STOPPED;
            }
            continue;
        }

         int samples = r / sizeof(int16_t);
        // for (int i = 0; i < samples; i++)
        // {
        //     int32_t s = (int32_t)((float)file_buf[i] * s_volume);
        //     s = (s > 32767) ? 32767 : (s < -32768 ? -32768 : s);
        //     int16_t out = (int16_t)s;
        //     i2s_buf[i * 2 + 0] = out; // L
        //     i2s_buf[i * 2 + 1] = out; // R
        // }


        uint8_t *p = (uint8_t *)file_buf;
int frames = 0;

if (s_wav_bits == 8) {
    frames = r / s_wav_channels;
} else if (s_wav_bits == 16) {
    frames = r / (2 * s_wav_channels);
} else if (s_wav_bits == 24) {
    frames = r / (3 * s_wav_channels);
} else {
    ESP_LOGE(TAG, "Unsupported bit depth: %u", s_wav_bits);
    continue;
}

for (int i = 0; i < frames; i++) {
    int16_t L = 0, R = 0;

    if (s_wav_bits == 8) {
        uint8_t u = p[i * s_wav_channels];
int16_t v = ((int)u - 128) << 8;
L = R = v;


    }
    else if (s_wav_bits == 16) {
        int16_t *q = (int16_t *)p;
        if (s_wav_channels == 1) {
            L = R = q[i];
        } else {
            L = q[i * 2];
            R = q[i * 2 + 1];
        }
    }
    else if (s_wav_bits == 24) {
       uint8_t *q = p + i * s_wav_channels * 3;

int32_t vl = (q[2] << 24) | (q[1] << 16) | (q[0] << 8);
vl >>= 16;
L = (int16_t)vl;

if (s_wav_channels == 2) {
    q += 3;
    int32_t vr = (q[2] << 24) | (q[1] << 16) | (q[0] << 8);
    vr >>= 16;
    R = (int16_t)vr;
} else {
    R = L;
}

    }

    L = (int16_t)((float)L * s_volume);
    R = (int16_t)((float)R * s_volume);

    i2s_buf[i * 2]     = L;
    i2s_buf[i * 2 + 1] = R;
}



        size_t bytes_written = 0;
        size_t out_bytes = frames * 2 * sizeof(int16_t);

i2s_channel_write(
    s_tx_chan,
    i2s_buf,
    out_bytes,
    &bytes_written,
    portMAX_DELAY
);

    }

    free(file_buf);
    free(i2s_buf);
}

/* ===== 公共 API ===== */

// 在外部创建任务
static StaticTask_t *pxTaskBuffer = NULL;
static StackType_t *puxStackBuffer = NULL;
TaskHandle_t wavTaskHandle = NULL;
#define WAV_STACK_SIZE 8192

void wav_start_task(void)
{

    // 在psram创建任务

    pxTaskBuffer = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    // 3. 在 PSRAM 为 Stack 申请内存（Stack 很大，放外部省空间）
    puxStackBuffer = (StackType_t *)heap_caps_malloc(WAV_STACK_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (pxTaskBuffer == NULL || puxStackBuffer == NULL)
    {
        ESP_LOGE("TASK", "内存不足，无法创建任务！");
        return;
    }
    wavTaskHandle = xTaskCreateStatic(
        wav_play_task,      // 任务函数
        "cube_static_task", // 任务名
        WAV_STACK_SIZE,     // 栈深度（字节）
        NULL,               // 参数
        4,                  // 优先级
        puxStackBuffer,     // 栈指向 PSRAM
        pxTaskBuffer        // TCB 指向内部 RAM
    );
}

esp_err_t wav_player_init(const char *playlist[], float volume)
{

    // 在 wav_player_init 中申请
    s_lrc_lines = heap_caps_malloc(sizeof(lrc_line_t) * MAX_LRC_LINES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    // 计算歌单长度
    int len = 0;
    while (playlist[len] != NULL)
        len++;
    if (len == 0)
        return ESP_ERR_INVALID_ARG;

    s_playlist = playlist;
    s_playlist_len = len;
    s_current_index = 0;
    s_volume = (volume < 0.0f) ? 0.0f : (volume > 1.0f ? 1.0f : volume);

    // 创建互斥锁
    s_sd_mutex = xSemaphoreCreateMutex();
    if (!s_sd_mutex)
    {
        ESP_LOGE(TAG, "Failed to create SD mutex");
        return ESP_FAIL;
    }

    // 创建命令队列
    s_cmd_queue = xQueueCreate(10, sizeof(player_cmd_msg_t));
    if (!s_cmd_queue)
    {
        ESP_LOGE(TAG, "Failed to create command queue");
        return ESP_FAIL;
    }

    s_ui_queue = xQueueCreate(8, sizeof(wav_player_ui_msg_t));
    if (!s_ui_queue)
    {
        ESP_LOGE(TAG, "Failed to create UI queue");
    }

    // 初始化 I2S
    ESP_ERROR_CHECK(i2s_init());

    // 启动播放任务
    // xTaskCreatePinnedToCore(wav_play_task, "wav_player", 8192, NULL, 7, NULL,0);

    // 移动任务到psram
    wav_start_task();

    ESP_LOGI(TAG, "WAV player initialized with %d tracks", len);
    return ESP_OK;
}

esp_err_t wav_player_send_cmd(wav_player_cmd_t cmd)
{
    if (!s_cmd_queue)
    {
        return ESP_ERR_INVALID_STATE;
    }
    player_cmd_msg_t msg = {
        .cmd = cmd,
        .volume = 0.0f,
        .index = -1,
    };
    if (xQueueSend(s_cmd_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t wav_player_set_volume_cmd(float volume)
{
    if (!s_cmd_queue)
        return ESP_ERR_INVALID_STATE;
    player_cmd_msg_t msg = {
        .cmd = WAV_CMD_SET_VOLUME,
        .volume = (volume < 0.0f) ? 0.0f : (volume > 1.0f ? 1.0f : volume),
        .index = -1,
    };
    if (xQueueSend(s_cmd_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t wav_player_play_index(int index)
{
    if (!s_cmd_queue)
        return ESP_ERR_INVALID_STATE;
    player_cmd_msg_t msg = {
        .cmd = WAV_CMD_PLAY_INDEX,
        .volume = 0.0f,
        .index = index,
    };
    if (xQueueSend(s_cmd_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

void wav_player_set_volume(float volume)
{
    s_volume = (volume < 0.0f) ? 0.0f : (volume > 1.0f ? 1.0f : volume);
}

bool wav_player_ui_dequeue(wav_player_ui_msg_t *out)
{
    if (!s_ui_queue || !out)
    {
        return false;
    }
    return xQueueReceive(s_ui_queue, out, 0) == pdTRUE;
}
