#include "wifi_time.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "esp_netif_sntp.h"
#include "vars.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ================= 配置 =================
#define NVS_NS_WIFI "wifi_cfg"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASS "pass"

#define SNTP_SERVER "ntp1.aliyun.com"

// ================= 日志 =================
static const char *TAG = "wifi_time";

// ================= 状态 =================
static bool s_sys_inited = false;
static esp_netif_t *s_sta_netif = NULL;

// UI 接口
extern void set_var_time_txt(const char *value);

// ------------------------------------------------
// WiFi 事件
// ------------------------------------------------
static void wifi_event_handler(void *arg,
                               esp_event_base_t base,
                               int32_t id,
                               void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "WiFi start -> connect");
        esp_wifi_connect();
    }

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "WiFi disconnected -> retry");
        esp_wifi_connect();
    }
}

// 创建一个队列
static QueueHandle_t s_ui_time_queue = NULL;

void ui_time_queue_init(void)
{
    if (!s_ui_time_queue)
    {
        s_ui_time_queue = xQueueCreate(4, sizeof(ui_time_msg_t));
    }
}

bool ui_time_enqueue(const ui_time_msg_t *msg)
{
    if (!s_ui_time_queue)
        return false;
    return xQueueSend(s_ui_time_queue, msg, 0) == pdTRUE;
}

bool ui_time_dequeue(ui_time_msg_t *out)
{
    if (!s_ui_time_queue || !out)
    {
        return false;
    }
    return xQueueReceive(s_ui_time_queue, out, 0) == pdTRUE;
}

// ------------------------------------------------
// SNTP 回调
// ------------------------------------------------
static void sntp_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP synchronized");
}

// ------------------------------------------------
// 读取 WiFi 配置（NVS）
// ------------------------------------------------
static esp_err_t load_wifi_from_nvs(char *ssid, size_t ssid_len,
                                    char *pass, size_t pass_len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS_WIFI, NVS_READONLY, &h);
    if (err != ESP_OK)
        return err;

    err = nvs_get_str(h, NVS_KEY_SSID, ssid, &ssid_len);
    if (err != ESP_OK)
        goto out;

    err = nvs_get_str(h, NVS_KEY_PASS, pass, &pass_len);

out:
    nvs_close(h);
    return err;
}

// ------------------------------------------------
// 写入 WiFi 配置（NVS）
// ------------------------------------------------
void wifi_time_set_ap(const char *ssid, const char *password)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NVS_NS_WIFI, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_str(h, NVS_KEY_SSID, ssid));
    ESP_ERROR_CHECK(nvs_set_str(h, NVS_KEY_PASS, password));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

    ESP_LOGI(TAG, "WiFi AP saved to NVS: %s", ssid);
}

// ------------------------------------------------
// 系统级初始化（只做一次）
// ------------------------------------------------
void wifi_time_sys_init(void)
{

    // 设置时区
    setenv("TZ", "CST-8", 1);
    tzset();

    if (s_sys_inited)
        return;
    s_sys_inited = true;

    ESP_LOGI(TAG, "wifi_time_sys_init");

    // NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // netif / event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();

    // WiFi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL));
}

// ------------------------------------------------
// 执行一次 SNTP 校时（按钮调用）
// ------------------------------------------------
void wifi_time_sync_now(void)
{
    char ssid[32] = {0};
    char pass[64] = {0};

    if (load_wifi_from_nvs(ssid, sizeof(ssid),
                           pass, sizeof(pass)) != ESP_OK)
    {
        ESP_LOGE(TAG, "No WiFi config in NVS");
        set_var_time_txt("No WiFi config");
        return;
    }

    ESP_LOGI(TAG, "Start SNTP sync via %s", ssid);

    wifi_config_t cfg = {0};
    cfg.sta.listen_interval = 3;
    strcpy((char *)cfg.sta.ssid, ssid);
    strcpy((char *)cfg.sta.password, pass);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());


    // 低功耗
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    ESP_ERROR_CHECK(esp_wifi_set_inactive_time(WIFI_IF_STA, 6));

    // SNTP
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER);
    sntp.sync_cb = sntp_cb;
    esp_netif_sntp_init(&sntp);

    time_t now;
    struct tm tm;
    int retry = 0;


     ui_time_msg_t msg  ; 

    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) == ESP_ERR_TIMEOUT &&
           retry++ < 10)
    {

        ESP_LOGI(TAG, "Waiting SNTP... %d", retry);

        char wifiinfo[64];

        snprintf(
            wifiinfo,
            sizeof(wifiinfo),
            "Waiting SNTP... %d", retry);

            msg.type = UI_TIME_SYNCING ; // 
             strncpy(msg.updating_msg, wifiinfo, sizeof(msg.updating_msg) - 1);
                ui_time_enqueue(&msg); // 更新字符串

        // set_var_cali_status(wifiinfo);
    }

    time(&now);
    localtime_r(&now, &tm);

   

    if (tm.tm_year < (2016 - 1900))
    {
        // 校准失败
        // set_var_time_txt("SNTP failed");
        msg.type = UI_TIME_SYNC_FAIL ; 

        ui_time_enqueue(&msg);
    }
    else
    {
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);

        // set_var_time_txt(buf);
        msg.type = UI_TIME_SYNC_OK; // 校准成功了
        ui_time_enqueue(&msg);      // 发送成功信息
    }

    // 关闭 WiFi（省电 & 防 OOM）
    esp_netif_sntp_deinit();
    esp_wifi_stop();

    // esp_wifi_deinit();

    ESP_LOGI(TAG, "WiFi stopped after sync");
}

static lv_timer_t *s_time_timer = NULL;

static void ui_time_timer_cb(lv_timer_t *t)
{
    time_t now;
    struct tm tm;
    char buf[32];

    time(&now);
    localtime_r(&now, &tm);

    if (tm.tm_year < (2016 - 1900))
    {
        // 时间还没校准，不显示
        return;
    }

    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    set_var_time_txt(buf);

    ui_time_msg_t msg = {0};
    msg.type = UI_TIME_UPDATE;
    strncpy(msg.time_str, buf, sizeof(msg.time_str) - 1);
    ui_time_enqueue(&msg);

    ESP_LOGI("RTC", "%s", buf);
}

void ui_time_timer_start(void)
{
    if (s_time_timer)
        return; // 防止重复创建

    // 每 1000 ms 刷新一次 UI 时间
    lvgl_port_lock(0);
    s_time_timer = lv_timer_create(ui_time_timer_cb, 1000, NULL);
    lvgl_port_unlock();
}

// 全局信号量
SemaphoreHandle_t s_time_sync_sem = NULL;

static void wifi_time_task(void *arg)
{
    for (;;)
    {
        // 等待按钮信号（永久阻塞，不占 CPU）
        xSemaphoreTake(s_time_sync_sem, portMAX_DELAY);

        ESP_LOGI("wifi_time", "Time sync triggered");
        wifi_time_sync_now(); // 阻塞也没关系
    }
}

void wifi_time_task_init(void)
{
    s_time_sync_sem = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(
        wifi_time_task,
        "wifi_time_task",
        4096, // WiFi 必须大栈
        NULL,
        5,
        NULL,
        0 // Core 0（WiFi 必须）
    );
}
