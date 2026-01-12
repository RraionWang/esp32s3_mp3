// bt.c
#include "bt.h"

#include "esp_log.h"
#include "nvs_flash.h"

/* NimBLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "freertos/task.h"

static const char *TAG = "BT_SCAN";

/* ================= 前置声明 ================= */
static void ble_host_task(void *param);
static void ble_on_sync(void);
static void ble_on_reset(int reason);
static int  gap_event_handler(struct ble_gap_event *event, void *arg);

/* ================= GAP 回调 ================= */
static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {

    case BLE_GAP_EVENT_DISC: {
        struct ble_hs_adv_fields fields;
        char name[32] = {0};

        /* 解析广播数据 */
        if (ble_hs_adv_parse_fields(&fields,
                                    event->disc.data,
                                    event->disc.length_data) == 0) {

            if (fields.name && fields.name_len < sizeof(name)) {
                memcpy(name, fields.name, fields.name_len);
            }
        }

        ESP_LOGI(TAG,
                 "Found device: %02X:%02X:%02X:%02X:%02X:%02X  RSSI=%d  Name=%s",
                 event->disc.addr.val[5],
                 event->disc.addr.val[4],
                 event->disc.addr.val[3],
                 event->disc.addr.val[2],
                 event->disc.addr.val[1],
                 event->disc.addr.val[0],
                 event->disc.rssi,
                 name[0] ? name : "<unknown>");
        return 0;
    }

    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "Scan complete");
        return 0;

    default:
        return 0;
    }
}

/* ================= 扫描函数 ================= */
void bt_start_scan(void)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params params = {0};

    ble_hs_id_infer_auto(0, &own_addr_type);

    params.passive = 1;           // 被动扫描
    params.filter_duplicates = 1; // 去重
    params.itvl = 0;
    params.window = 0;

    ESP_LOGI(TAG, "Start BLE scan");

    ble_gap_disc(own_addr_type,
                 BLE_HS_FOREVER,
                 &params,
                 gap_event_handler,
                 NULL);
}

/* ================= NimBLE 生命周期 ================= */
static void ble_on_sync(void)
{
    ESP_LOGI(TAG, "BLE synced");
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE reset, reason=%d", reason);
}

static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ================= 对外初始化接口 ================= */
void bt_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(nimble_port_init());

    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb  = ble_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_svc_gap_init();
    ble_svc_gap_device_name_set("ESP32S3-SCANNER");

    nimble_port_freertos_init(ble_host_task);
}


TaskHandle_t bt_task_handle = NULL;

 void bt_task(void *arg)
{
    uint32_t notify_value;

    for (;;) {

        /* 等待按钮通知 */
        xTaskNotifyWait(
            0,              // 不清除进入标志
            0,              // 不清除退出标志
            &notify_value,
            portMAX_DELAY   // 一直等
        );

        ESP_LOGI("BT", "Scan button pressed, start scan");


     

        bt_start_scan();   // 👉 你之前写的 NimBLE 扫描函数
    }
}
