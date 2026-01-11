/*
 * QMI8658A driver - shared I2C bus version (ESP-IDF v5.x)
 * Output string -> set_var_qmi_text(const char *value)
 */

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "vars.h"
#include "screens.h"
#include "lvgl.h"
#include "draw_cube.h"
#include "vars.h"





// acc 加速度
// gyr 角速度
// angle 姿态角


static const char *TAG = "QMI8658";

/* ================= I2C ================= */
#define QMI8658_I2C_FREQ_HZ     100000
#define QMI8658_TIMEOUT_MS      1000

/* 教程常见地址是 0x6A；也有人焊成 0x6B，所以做双探测 */
#define QMI8658_ADDR_0          0x6A
#define QMI8658_ADDR_1          0x6B

/* ================= 寄存器（关键） ================= */
#define QMI8658_REG_WHO_AM_I    0x00

#define QMI8658_REG_CTRL1       0x02
#define QMI8658_REG_CTRL2       0x03
#define QMI8658_REG_CTRL3       0x04
#define QMI8658_REG_CTRL7       0x08

#define QMI8658_REG_STATUS0     0x2E    // aDA/gDA 就绪位 :contentReference[oaicite:1]{index=1}
#define QMI8658_REG_AX_L        0x35    // AX_L 起始，后面连续 AY/AZ/GX/GY/GZ :contentReference[oaicite:2]{index=2}
#define QMI8658_REG_RESET       0x60    // 写 0xB0 触发软复位 :contentReference[oaicite:3]{index=3}

/* WHO_AM_I 教程常见期望是 0x05（有些批次/兼容器件可能不同）
 * 所以这里不把 0x05 当“唯一正确”，只要能稳定读出就认为在线。
 */
#define QMI8658_WHOAMI_HINT     0x05

/* ================= 设备句柄 ================= */
static i2c_master_dev_handle_t s_qmi_dev = NULL;
static uint8_t s_qmi_addr = 0x00;

/* ================= 输出字符串 ================= */
static char s_qmi_str[128];

/* =========================================================
 * I2C 读/写
 * ========================================================= */
static esp_err_t qmi_reg_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(
        s_qmi_dev,
        &reg,
        1,
        data,
        len,
        QMI8658_TIMEOUT_MS
    );
}

static esp_err_t qmi_reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(
        s_qmi_dev,
        buf,
        sizeof(buf),
        QMI8658_TIMEOUT_MS
    );

  
}

/* =========================================================
 * 探测地址：0x6A / 0x6B
 * - 能读 WHO_AM_I 就认为在线
 * ========================================================= */
static esp_err_t qmi_probe_addr(i2c_master_bus_handle_t bus, uint8_t addr, uint8_t *whoami_out)
{
    i2c_master_dev_handle_t dev = NULL;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = QMI8658_I2C_FREQ_HZ,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &dev);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t who = 0x00;
    uint8_t reg = QMI8658_REG_WHO_AM_I;
    ret = i2c_master_transmit_receive(dev, &reg, 1, &who, 1, QMI8658_TIMEOUT_MS);

    if (ret == ESP_OK) {
        *whoami_out = who;

        // 保存为全局句柄（把老的删掉，换成新的）
        if (s_qmi_dev) {
            i2c_master_bus_rm_device(s_qmi_dev);
            s_qmi_dev = NULL;
        }
        s_qmi_dev  = dev;
        s_qmi_addr = addr;
        return ESP_OK;
    }

    // 失败就清理掉
    i2c_master_bus_rm_device(dev);
    return ret;
}

/* =========================================================
 * 初始化：参考教程的写法
 * 1) WHO_AM_I
 * 2) RESET(0x60)=0xB0
 * 3) CTRL1=0x40 (auto increment)
 * 4) CTRL7=0x03 (enable accel + gyro)
 * 5) CTRL2=0x95 (acc config)
 * 6) CTRL3=0xD5 (gyro config)
 * ========================================================= */
esp_err_t qmi8658_init(i2c_master_bus_handle_t bus)
{
    if (bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t who = 0;

    // 先试 0x6A，再试 0x6B
    esp_err_t ret = qmi_probe_addr(bus, QMI8658_ADDR_0, &who);
    if (ret != ESP_OK) {
        ret = qmi_probe_addr(bus, QMI8658_ADDR_1, &who);
    }
    // ESP_RETURN_ON_ERROR(ret, TAG, "QMI8658 probe failed (0x6A/0x6B both NACK?)");

    ESP_LOGI(TAG, "QMI8658 found at 0x%02X, WHO_AM_I=0x%02X (hint=0x%02X)",
             s_qmi_addr, who, QMI8658_WHOAMI_HINT);

    // 软复位：RESET(0x60)=0xB0 :contentReference[oaicite:4]{index=4}
    ESP_ERROR_CHECK(qmi_reg_write(QMI8658_REG_RESET, 0xB0));
    vTaskDelay(pdMS_TO_TICKS(15));  // datasheet：复位最长约 15ms :contentReference[oaicite:5]{index=5}

    // CTRL1=0x40：教程用于“地址自动递增”等设置（跟教程一致）
    ESP_ERROR_CHECK(qmi_reg_write(QMI8658_REG_CTRL1, 0x40));
    vTaskDelay(pdMS_TO_TICKS(2));

    // CTRL7=0x03：使能 accel + gyro
    ESP_ERROR_CHECK(qmi_reg_write(QMI8658_REG_CTRL7, 0x03));
    vTaskDelay(pdMS_TO_TICKS(2));

    // CTRL2=0x95：教程中的加速度配置
    ESP_ERROR_CHECK(qmi_reg_write(QMI8658_REG_CTRL2, 0x95));
    vTaskDelay(pdMS_TO_TICKS(2));

    // CTRL3=0xD5：教程中的陀螺仪配置
    ESP_ERROR_CHECK(qmi_reg_write(QMI8658_REG_CTRL3, 0xD5));
    vTaskDelay(pdMS_TO_TICKS(2));

    ESP_LOGI(TAG, "✅ QMI8658 init OK (shared I2C)");
    return ESP_OK;
}

/* =========================================================
 * 读原始数据（带 STATUS0 就绪判断）
 * ========================================================= */
static esp_err_t qmi_read_raw(int16_t *ax, int16_t *ay, int16_t *az,
                             int16_t *gx, int16_t *gy, int16_t *gz)
{
    // 1) 读 STATUS0：bit0=aDA, bit1=gDA :contentReference[oaicite:6]{index=6}
    uint8_t st = 0;
    esp_err_t ret = qmi_reg_read(QMI8658_REG_STATUS0, &st, 1);
    if (ret != ESP_OK) return ret;

    if ((st & 0x03) != 0x03) {
        // accel 或 gyro 其中一个还没更新
        return ESP_ERR_INVALID_STATE;
    }

    // 2) 从 AX_L(0x35) 连读 12 字节：AX/AY/AZ/GX/GY/GZ :contentReference[oaicite:7]{index=7}
    uint8_t buf[12] = {0};
    ret = qmi_reg_read(QMI8658_REG_AX_L, buf, sizeof(buf));
    if (ret != ESP_OK) return ret;

    *ax = (int16_t)((buf[1] << 8) | buf[0]);
    *ay = (int16_t)((buf[3] << 8) | buf[2]);
    *az = (int16_t)((buf[5] << 8) | buf[4]);

    *gx = (int16_t)((buf[7] << 8) | buf[6]);
    *gy = (int16_t)((buf[9] << 8) | buf[8]);
    *gz = (int16_t)((buf[11] << 8) | buf[10]);

    return ESP_OK;
}

/* =========================================================
 * QMI8658 任务：输出字符串到 vars
 * ========================================================= */
static void qmi_task(void *arg)
{
    (void)arg;

    int16_t ax, ay, az;
    int16_t gx, gy, gz;

    while (1) {
        esp_err_t ret = qmi_read_raw(&ax, &ay, &az, &gx, &gy, &gz);
        if (ret == ESP_OK) {

            // 注意：下面这个换算比例是否与你 CTRL2/CTRL3 的量程完全一致，
            // 取决于你 CTRL2/CTRL3 具体配置（教程里是固定写死的）。
            // 这里先给一个“能用/好看”的显示：保留原始值 + 计算姿态角

            // 用加速度算 Pitch/Roll（教程常见做法）
            float axf = (float)ax;
            float ayf = (float)ay;
            float azf = (float)az;

            float roll  = atanf(ayf / sqrtf(axf * axf + azf * azf)) * 57.2958f;
            float pitch = atanf(-axf / sqrtf(ayf * ayf + azf * azf)) * 57.2958f;

            // 拼字符串给 UI
            snprintf(
                s_qmi_str,
                sizeof(s_qmi_str),
             
                "ACC:X:%d Y:%d Z:%d\n"
                "GYR:X:%d Y:%d Z:%d\n"
                "Pitch:%.2f Roll :%.2f",
              
                ax, ay, az,
                gx, gy, gz,
                pitch, roll
            );



            // ⚠️ 这里用 set_var（你之前写成 get_var_qmi_text 可能是手滑）
            set_var_qmi_text(s_qmi_str);



            // 在这里更新pitch和rool
            // draw_cube_set_angle(pitch, roll);


            // 更新
            set_var_pitch_var(pitch) ;
            set_var_roll_var(roll) ; 

            




            // 打印
            // ESP_LOGI(TAG, "%s", s_qmi_str);

        } else if (ret != ESP_ERR_INVALID_STATE) {
            // INVALID_STATE = 数据没就绪，不算错误
            ESP_LOGW(TAG, "qmi_read_raw failed: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}



void qmi8658_start_task(void)
{
    xTaskCreate(qmi_task, "qmi8658_task", 2048, NULL, 5, NULL);
}



