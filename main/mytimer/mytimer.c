#include "mytimer.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gptimer.h"
#include "esp_log.h"

static const char *TAG = "mytimer";

/* ================== 内部变量 ================== */

typedef struct {
    uint64_t count;
} timer_evt_t;

static gptimer_handle_t s_timer = NULL;
static QueueHandle_t    s_queue = NULL;
static mytimer_cb_t     s_user_cb = NULL;
static TaskHandle_t     s_task_handle = NULL;

/* ================== ISR 回调 ================== */

static bool IRAM_ATTR mytimer_isr_cb(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    QueueHandle_t q = (QueueHandle_t)user_data;

    timer_evt_t evt = {
        .count = edata->count_value
    };

    xQueueSendFromISR(q, &evt, &high_task_awoken);
    return high_task_awoken == pdTRUE;
}

/* ================== 任务函数 ================== */

static void mytimer_task(void *arg)
{
    timer_evt_t evt;

    while (1) {
        if (xQueueReceive(s_queue, &evt, portMAX_DELAY)) {
            if (s_user_cb) {
                s_user_cb(evt.count);   // 用户回调（安全）
            }
        }
    }
}













// 在外部创建任务
static StaticTask_t *pxTaskBuffer = NULL;
static StackType_t  *puxStackBuffer = NULL;

#define MYTIMER_STACK_SIZE 4096



void mytimer_start_task(void)
{
   


// 在psram创建任务

    pxTaskBuffer = (StaticTask_t *)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    // 3. 在 PSRAM 为 Stack 申请内存（Stack 很大，放外部省空间）
    puxStackBuffer = (StackType_t *)heap_caps_malloc(MYTIMER_STACK_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (pxTaskBuffer == NULL || puxStackBuffer == NULL) {
        ESP_LOGE("TASK", "内存不足，无法创建任务！");
        return;
    }
        s_task_handle = xTaskCreateStatic(
        mytimer_task,   // 任务函数
        "my_timer_static_task",     // 任务名
        MYTIMER_STACK_SIZE,        // 栈深度（字节）
        NULL,                   // 参数
        4,                      // 优先级
        puxStackBuffer,         // 栈指向 PSRAM
        pxTaskBuffer            // TCB 指向内部 RAM
    );



}







/* ================== 对外接口 ================== */

bool mytimer_start(uint64_t period_us, mytimer_cb_t cb)
{
    if (s_timer) {
        ESP_LOGW(TAG, "timer already started");
        return false;
    }

    s_user_cb = cb;

    /* 创建队列 */
    s_queue = xQueueCreate(4, sizeof(timer_evt_t));
    if (!s_queue) {
        ESP_LOGE(TAG, "queue create failed");
        return false;
    }

    // /* 创建处理任务 */
    // xTaskCreate(
    //     mytimer_task,
    //     "mytimer_task",
    //     4096,
    //     NULL,
    //     10,
    //     &s_task_handle
    // );


// 使用psram中的静态任务来代替
    mytimer_start_task() ;


    /* 创建 GPTimer */
    gptimer_config_t cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&cfg, &s_timer));

    /* 注册 ISR */
    gptimer_event_callbacks_t cbs = {
        .on_alarm = mytimer_isr_cb,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(s_timer, &cbs, s_queue));

    /* 设置周期 */
    gptimer_alarm_config_t alarm_cfg = {
        .reload_count = 0,
        .alarm_count = period_us,
        .flags.auto_reload_on_alarm = true,
    };

    ESP_ERROR_CHECK(gptimer_enable(s_timer));
    ESP_ERROR_CHECK(gptimer_set_alarm_action(s_timer, &alarm_cfg));
    ESP_ERROR_CHECK(gptimer_start(s_timer));

    ESP_LOGI(TAG, "timer started: %llu us", period_us);
    return true;
}

void mytimer_stop(void)
{
    if (!s_timer) return;

    ESP_LOGI(TAG, "timer stop");

    gptimer_stop(s_timer);
    gptimer_disable(s_timer);
    gptimer_del_timer(s_timer);
    s_timer = NULL;

    if (s_task_handle) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }

    if (s_queue) {
        vQueueDelete(s_queue);
        s_queue = NULL;
    }

    s_user_cb = NULL;
}

