#include "wifi_time.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "esp_http_server.h"
#include "esp_netif_sntp.h"

#include "vars.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

/* =========================================================
 * 说明：
 *  - 本文件把原 main.c 的“Web 配网 + WiFi 扫描 + APSTA 切 STA”
 *    合并进来，供你在项目里直接调用。
 *  - 你可以在调用 wifi_prov_run() 时自定义 SoftAP 密码；
 *    从网页提交的 SSID/Password 会自动写入 NVS（wifi_cfg）。
 * ========================================================= */

/* ================= NVS 配置 ================= */
#define NVS_NS_WIFI   "wifi_cfg"
#define NVS_KEY_SSID  "ssid"
#define NVS_KEY_PASS  "pass"

/* ================= SNTP 配置 ================= */
#define SNTP_SERVER "ntp1.aliyun.com"

/* ================= 日志 ================= */
static const char *TAG = "wifi_time";


#define WIFI_PROV_NAMESPACE "wifi_prov"
#define WIFI_PROV_KEY       "done"

 

/* ================= HTML（用 component embedding）================= */
/* 你把 wifi.html 放到 main/ 并在 CMakeLists.txt 用 target_add_binary_data() 加载即可 */
extern const uint8_t wifi_html_start[] asm("_binary_wifi_html_start");
extern const uint8_t wifi_html_end[]   asm("_binary_wifi_html_end");

/* ================= 状态 ================= */
static bool s_sys_inited = false;
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif  = NULL;

/* 仅在我们明确需要连接时才自动 esp_wifi_connect() */
static volatile bool s_sta_autoconnect = false;

/* WiFi 连接事件 */
static EventGroupHandle_t s_wifi_ev = NULL;
#define WIFI_GOT_IP_BIT BIT0

/* Web 配网全局 */
static httpd_handle_t s_httpd = NULL;
static volatile bool  s_wifi_info_received = false;
static char s_prov_ssid[32] = {0};
static char s_prov_pass[64] = {0};

/* UI 接口（你项目里已有） */
extern void set_var_time_txt(const char *value);

/* =========================================================
 * UI Time Queue（保持你原来的逻辑）
 * ========================================================= */






static QueueHandle_t s_ui_time_queue = NULL;

void ui_time_queue_init(void)
{
    if (!s_ui_time_queue) {
        s_ui_time_queue = xQueueCreate(4, sizeof(ui_time_msg_t));
    }
}

bool ui_time_enqueue(const ui_time_msg_t *msg)
{
    if (!s_ui_time_queue) return false;
    return xQueueSend(s_ui_time_queue, msg, 0) == pdTRUE;
}

bool ui_time_dequeue(ui_time_msg_t *out)
{
    if (!s_ui_time_queue || !out) return false;
    return xQueueReceive(s_ui_time_queue, out, 0) == pdTRUE;
}

/* =========================================================
 * WiFi / IP 事件
 * ========================================================= */
static void wifi_event_handler(void *arg,
                               esp_event_base_t base,
                               int32_t id,
                               void *data)
{
    (void)arg;
    (void)data;

    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
            if (s_sta_autoconnect) {
                ESP_LOGI(TAG, "Auto connect enabled -> esp_wifi_connect()");
                esp_wifi_connect();
            }
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED");
            xEventGroupClearBits(s_wifi_ev, WIFI_GOT_IP_BIT);
            if (s_sta_autoconnect) {
                ESP_LOGI(TAG, "Retry connect...");
                esp_wifi_connect();
            }
        }
    }

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_ev, WIFI_GOT_IP_BIT);
    }
}

/* =========================================================
 * SNTP 回调
 * ========================================================= */
static void sntp_cb(struct timeval *tv)
{
    (void)tv;
    ESP_LOGI(TAG, "SNTP synchronized");
}

/* =========================================================
 * 读取 / 写入 WiFi 配置（NVS）
 * ========================================================= */
static esp_err_t load_wifi_from_nvs(char *ssid, size_t ssid_len,
                                   char *pass, size_t pass_len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS_WIFI, NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    err = nvs_get_str(h, NVS_KEY_SSID, ssid, &ssid_len);
    if (err != ESP_OK) {
        nvs_close(h);
        return err;
    }

    err = nvs_get_str(h, NVS_KEY_PASS, pass, &pass_len);
    nvs_close(h);
    return err;
}

void wifi_time_set_ap(const char *ssid, const char *password)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NVS_NS_WIFI, NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_str(h, NVS_KEY_SSID, ssid));
    ESP_ERROR_CHECK(nvs_set_str(h, NVS_KEY_PASS, password));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

    ESP_LOGI(TAG, "WiFi saved to NVS: ssid=%s", ssid);
}

/* =========================================================
 * 系统级初始化（只做一次）
 * ========================================================= */
void wifi_time_sys_init(void)
{
    /* 设置时区（UTC+8） */
    setenv("TZ", "CST-8", 1);
    tzset();

    if (s_sys_inited) return;
    s_sys_inited = true;

    ESP_LOGI(TAG, "wifi_time_sys_init");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* netif / event loop */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 事件组 */
    s_wifi_ev = xEventGroupCreate();
    if (!s_wifi_ev) {
        ESP_LOGE(TAG, "Failed to create event group");
        abort();
    }

    /* 默认 STA + AP netif（Web 配网需要 AP） */
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif  = esp_netif_create_default_wifi_ap();
    (void)s_sta_netif;
    (void)s_ap_netif;

    /* WiFi driver */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* 事件回调 */
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
}



/* =========================================================
 * 检查是否配过网了
 * ========================================================= */
 static void wifi_mark_provisioned(void)
{
    nvs_handle_t nvs;
    nvs_open(WIFI_PROV_NAMESPACE, NVS_READWRITE, &nvs);
    nvs_set_u8(nvs, WIFI_PROV_KEY, 1);
    nvs_commit(nvs);
    nvs_close(nvs);
}

static bool wifi_is_provisioned(void)
{
    nvs_handle_t nvs;
    uint8_t done = 0;

    if (nvs_open(WIFI_PROV_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK)
        return false;

    nvs_get_u8(nvs, WIFI_PROV_KEY, &done);
    nvs_close(nvs);

    return done == 1;
}


/* =========================================================
 * 重新配网函数
 * ========================================================= */



  TaskHandle_t wifi_ctrl_task = NULL;






static void wifi_reprovision_task(void *arg)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        ESP_LOGI("WIFI", "执行重新配网（后台任务）");
        wifi_reprovision();
    }
}



void wifi_reprovision(void)
{
    ESP_LOGI("WIFI", "清空 WiFi 配置，重新配网");

    esp_wifi_stop();
    esp_wifi_restore();   // ⭐ 清空 WiFi NVS（SSID / 密码）

    // 清除配网完成标志
    nvs_handle_t nvs;
    nvs_open(WIFI_PROV_NAMESPACE, NVS_READWRITE, &nvs);
    nvs_erase_key(nvs, WIFI_PROV_KEY);
    nvs_commit(nvs);
    nvs_close(nvs);

    // wifi_start_provision();


    wifi_prov_run("ESP32S3", "123456789");
    vTaskDelete(NULL);   // 配完就销毁


}


 void wifi_reprovision_task_init(void)
{
    xTaskCreate(
        wifi_reprovision_task,
        "wifi_ctrl",
        4096,
        NULL,
        5,
        &wifi_ctrl_task
    );
}






/* =========================================================
 * Web 配网：HTTP Handlers
 * ========================================================= */
static esp_err_t http_root_get(httpd_req_t *req)
{
    const size_t html_size = (size_t)(wifi_html_end - wifi_html_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)wifi_html_start, (ssize_t)html_size);
    return ESP_OK;
}

static esp_err_t http_save_post(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
        return ESP_FAIL;
    }
    buf[len] = 0;

    /* key 名必须与你网页提交一致：ssid / password */
    memset(s_prov_ssid, 0, sizeof(s_prov_ssid));
    memset(s_prov_pass, 0, sizeof(s_prov_pass));
    httpd_query_key_value(buf, "ssid", s_prov_ssid, sizeof(s_prov_ssid));
    httpd_query_key_value(buf, "password", s_prov_pass, sizeof(s_prov_pass));

    ESP_LOGI(TAG, "WEB_PROV: SSID: %s", s_prov_ssid);
    ESP_LOGI(TAG, "WEB_PROV: PASS: %s", s_prov_pass);



    /* 保存到 NVS（后续 SNTP / 自动连接都用它） */
    wifi_time_set_ap(s_prov_ssid, s_prov_pass);

    s_wifi_info_received = true;
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

static esp_err_t http_scan_get(httpd_req_t *req)
{
    /* APSTA 模式下也可以扫描，扫描用的是 STA 侧 */
    wifi_scan_config_t cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    esp_err_t err = esp_wifi_scan_start(&cfg, true); /* 阻塞直到完成 */
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "scan start failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "scan failed");
        return ESP_FAIL;
    }

    uint16_t ap_num = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));

    httpd_resp_set_type(req, "application/json");

    if (ap_num == 0) {
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }

    wifi_ap_record_t *aps = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_num);
    if (!aps) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no mem");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, aps));

    httpd_resp_sendstr_chunk(req, "[");
    for (int i = 0; i < ap_num; i++) {
        char line[192];
        /* 注意：ssid 里可能有引号等特殊字符，这里简单处理：遇到引号会破 JSON。
           如果你需要更严格的 JSON，后面我也可以给你加 escape。 */
        snprintf(line, sizeof(line),
                 "{\"ssid\":\"%s\",\"rssi\":%d}%s",
                 (char *)aps[i].ssid,
                 aps[i].rssi,
                 (i == ap_num - 1) ? "" : ",");
        httpd_resp_sendstr_chunk(req, line);
    }
    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, NULL);

    free(aps);
    return ESP_OK;
}

/* =========================================================
 * Web 配网：HTTP Server
 * ========================================================= */
static void web_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;

    ESP_ERROR_CHECK(httpd_start(&s_httpd, &cfg));

    httpd_uri_t u_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = http_root_get,
        .user_ctx = NULL
    };
    httpd_uri_t u_save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = http_save_post,
        .user_ctx = NULL
    };
    httpd_uri_t u_scan = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = http_scan_get,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(s_httpd, &u_root);
    httpd_register_uri_handler(s_httpd, &u_save);
    httpd_register_uri_handler(s_httpd, &u_scan);

    ESP_LOGI(TAG, "WEB_PROV: HTTP server started");
}

static void web_stop(void)
{
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
        ESP_LOGI(TAG, "WEB_PROV: HTTP server stopped");
    }
}

/* =========================================================
 * Web 配网：启动 SoftAP（允许你自定义密码）
 * ========================================================= */
static void prov_start_softap(const char *ap_ssid,
                              const char *ap_pass,
                              int channel,
                              int max_conn)
{
    wifi_config_t ap_cfg = {0};

    /* SSID */
    strncpy((char *)ap_cfg.ap.ssid, ap_ssid, sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len = (uint8_t)strlen(ap_ssid);

    /* 密码（>=8 才能 WPA2，否则 OPEN） */
    if (ap_pass && ap_pass[0] != '\0') {
        strncpy((char *)ap_cfg.ap.password, ap_pass, sizeof(ap_cfg.ap.password) - 1);
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    ap_cfg.ap.channel = channel;
    ap_cfg.ap.max_connection = max_conn;

    /* 重要：配网页面需要 AP + 扫描需要 STA，所以 APSTA */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));

    /* 启动 WiFi（只 start，不要 connect） */
    s_sta_autoconnect = false;
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WEB_PROV: SoftAP started: %s", ap_ssid);
    ESP_LOGI(TAG, "WEB_PROV: IP: 192.168.4.1");
}

/* =========================================================
 * Web 配网：切到 STA 并连接（用 NVS 保存的 WiFi）
 * ========================================================= */
static esp_err_t prov_switch_to_sta_and_connect(void)
{
    char ssid[32] = {0};
    char pass[64] = {0};

    if (load_wifi_from_nvs(ssid, sizeof(ssid), pass, sizeof(pass)) != ESP_OK) {
        ESP_LOGE(TAG, "WEB_PROV: No WiFi config in NVS");
        return ESP_FAIL;
    }

    set_var_ssid_txt(s_prov_ssid); 
    set_var_password_txt(s_prov_pass);


    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, pass, sizeof(sta_cfg.sta.password) - 1);

    /* 若 password 为空，让阈值允许 OPEN，否则会出现你之前那条 log 的问题 */
    if (pass[0] == '\0') {
        sta_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    xEventGroupClearBits(s_wifi_ev, WIFI_GOT_IP_BIT);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));

    s_sta_autoconnect = true;
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "WEB_PROV: STA connecting... (%s)", ssid);
    return ESP_OK;
}

/* =========================================================
 * 对外 API：运行一次 Web 配网（阻塞）
 *
 * 你可以在任意地方调用：
 *   wifi_time_sys_init();
 *   wifi_prov_run("ESP32-SETUP", "你自己设定的密码");
 *
 * 返回：
 *   ESP_OK         - 收到 WiFi 并已开始连接（并等待 Got IP 成功）
 *   ESP_ERR_TIMEOUT- 等待网页提交 / 等待 Got IP 超时
 *   其它           - ESP-IDF 错误码
 * ========================================================= */
esp_err_t wifi_prov_run(const char *ap_ssid, const char *ap_pass)
{
    if (!s_sys_inited) {
        ESP_LOGE(TAG, "Please call wifi_time_sys_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    /* 清状态 */
    s_wifi_info_received = false;
    memset(s_prov_ssid, 0, sizeof(s_prov_ssid));
    memset(s_prov_pass, 0, sizeof(s_prov_pass));

    /* 开 AP + Web */
    prov_start_softap(ap_ssid, ap_pass, 1, 4);
    web_start();

    /* 等网页提交（你也可以改成超时） */
    while (!s_wifi_info_received) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    /* 关 Web，切 STA */
    web_stop();
    ESP_LOGI(TAG, "WEB_PROV: Switching to STA mode");

    esp_err_t err = prov_switch_to_sta_and_connect();
    if (err != ESP_OK) return err;

    /* 等待 Got IP（建议给个超时，避免卡死） */
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_ev,
        WIFI_GOT_IP_BIT,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(20000)
    );

    if ((bits & WIFI_GOT_IP_BIT) == 0) {
        ESP_LOGW(TAG, "WEB_PROV: Wait Got IP timeout");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "WEB_PROV: Provisioning finished");
    return ESP_OK;
}

/* =========================================================
 * 执行一次 SNTP 校时（按钮调用）
 * ========================================================= */
void wifi_time_sync_now(void)
{
    char ssid[32] = {0};
    char pass[64] = {0};

    if (load_wifi_from_nvs(ssid, sizeof(ssid), pass, sizeof(pass)) != ESP_OK) {
        ESP_LOGE(TAG, "No WiFi config in NVS");
        set_var_time_txt("No WiFi config");
        return;
    }

    ESP_LOGI(TAG, "Start SNTP sync via %s", ssid);

    /* 配置 STA */
    wifi_config_t cfg = {0};
    cfg.sta.listen_interval = 3;
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, pass, sizeof(cfg.sta.password) - 1);

    if (pass[0] == '\0') {
        cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    /* 连接 */
    s_sta_autoconnect = true;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));

    /* 如果 WiFi 已经 start 过，esp_wifi_start() 会返回 ESP_ERR_WIFI_NOT_STOPPED，
       这里兼容一下： */
    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STOPPED) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
        return;
    }

    /* 低功耗（可按需删除） */
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    ESP_ERROR_CHECK(esp_wifi_set_inactive_time(WIFI_IF_STA, 6));

    /* SNTP */
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER);
    sntp.sync_cb = sntp_cb;
    esp_netif_sntp_init(&sntp);

    int retry = 0;
    ui_time_msg_t msg = {0};

    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) == ESP_ERR_TIMEOUT &&
           retry++ < 10) {

        ESP_LOGI(TAG, "Waiting SNTP... %d", retry);

        char wifiinfo[64];
        snprintf(wifiinfo, sizeof(wifiinfo), "Waiting SNTP... %d", retry);

        msg.type = UI_TIME_SYNCING;
        memset(msg.updating_msg, 0, sizeof(msg.updating_msg));
        strncpy(msg.updating_msg, wifiinfo, sizeof(msg.updating_msg) - 1);
        ui_time_enqueue(&msg);
    }

    time_t now;
    struct tm tm;
    time(&now);
    localtime_r(&now, &tm);

    if (tm.tm_year < (2016 - 1900)) {
        msg.type = UI_TIME_SYNC_FAIL;
        ui_time_enqueue(&msg);
    } else {
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);

        msg.type = UI_TIME_SYNC_OK;
        ui_time_enqueue(&msg);
    }

    /* 关闭 WiFi（省电 & 给其他任务让 RAM） */
    esp_netif_sntp_deinit();
    esp_wifi_stop();
    s_sta_autoconnect = false;

    ESP_LOGI(TAG, "WiFi stopped after sync");
}

/* =========================================================
 * LVGL 定时器：每秒刷新时间（保持你原来的逻辑）
 * ========================================================= */
static lv_timer_t *s_time_timer = NULL;

static void ui_time_timer_cb(lv_timer_t *t)
{
    (void)t;

    time_t now;
    struct tm tm;
    char buf[32];

    time(&now);
    localtime_r(&now, &tm);

    if (tm.tm_year < (2016 - 1900)) {
        return; /* 未校准不显示 */
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
    if (s_time_timer) return; /* 防止重复创建 */

    lvgl_port_lock(0);
    s_time_timer = lv_timer_create(ui_time_timer_cb, 1000, NULL);
    lvgl_port_unlock();
}

/* =========================================================
 * WiFi 时间同步任务（保持你原来的逻辑）
 * ========================================================= */
SemaphoreHandle_t s_time_sync_sem = NULL;

static void wifi_time_task(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        if (xSemaphoreTake(s_time_sync_sem, portMAX_DELAY) == pdTRUE) {
            wifi_time_sync_now();
        }
    }
}

void wifi_time_task_init(void)
{
    s_time_sync_sem = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(
        wifi_time_task,
        "wifi_time_task",
        8192 * 2,
        NULL,
        4,
        NULL,
        0 /* Core 0（WiFi 常在 0） */
    );
}

/* =========================================================
 * 独立 WiFi 扫描函数（保持你原来的逻辑，并避免误连接）
 * ========================================================= */
void wifi_scan_independent(void)
{
    ESP_LOGI(TAG, "Initializing independent scan...");

    s_sta_autoconnect = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STOPPED) {
        ESP_LOGE(TAG, "WiFi Start Failed: %s", esp_err_to_name(ret));
        return;
    }

    uint16_t number = 15;
    wifi_ap_record_t ap_info[15];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 250
    };

    ESP_LOGI(TAG, "Scanning for APs...");
    esp_err_t res = esp_wifi_scan_start(&scan_config, true);

    if (res == ESP_OK) {
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));

        ESP_LOGI(TAG, "------------------ Scan Results ------------------");
        ESP_LOGI(TAG, "%-32s | %-4s | %-4s | %-12s", "SSID", "RSSI", "CHN", "AUTH");

        for (int i = 0; i < number && i < ap_count; i++) {
            const char *auth_mode;
            switch (ap_info[i].authmode) {
                case WIFI_AUTH_OPEN: auth_mode = "OPEN"; break;
                case WIFI_AUTH_WPA_PSK: auth_mode = "WPA_PSK"; break;
                case WIFI_AUTH_WPA2_PSK: auth_mode = "WPA2_PSK"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: auth_mode = "WPA_WPA2"; break;
                default: auth_mode = "UNKNOWN"; break;
            }

            ESP_LOGI(TAG, "%-32s | %-4d | %-4d | %-12s",
                     (char *)ap_info[i].ssid,
                     ap_info[i].rssi,
                     ap_info[i].primary,
                     auth_mode);
        }
        ESP_LOGI(TAG, "--------------------------------------------------");
    } else {
        ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(res));
    }

    esp_wifi_stop();
    ESP_LOGI(TAG, "WiFi Stopped, RAM freed for other tasks.");
}



