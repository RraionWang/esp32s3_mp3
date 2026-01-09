#include "sd.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"

#define SD_USE_SPI 1 // 👈 在此切换：1=SPI, 0=SDIO(1-bit)

// 公共挂载点
#define MOUNT_POINT "/sdcard"

// 引脚定义（根据你的硬件）

#define SD_D3_GPIO 48  // SDIO: D3 (unused in 1-bit); SPI: CS
#define SD_CMD_GPIO 47 // SDIO: CMD; SPI: MOSI
#define SD_CLK_GPIO 21
#define SD_D0_GPIO 14  // SDIO: D0;  SPI: MISO


// ===================================================

static sdmmc_card_t *sdcard;
static const char *TAG = "sd";

void sdcard_init(void)
{
    esp_err_t ret;

#if SD_USE_SPI

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    
 

        spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_CMD_GPIO,
        .miso_io_num = SD_D0_GPIO,
        .sclk_io_num = SD_CLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };


      ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }


    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();

    slot_config.host_id = host.slot;

    slot_config.gpio_cs = SD_D3_GPIO;

     ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &sdcard);


      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card in SPI mode: %s", esp_err_to_name(ret));
        spi_bus_free(host.slot); // 可选：释放总线
        return; // 重要：不要继续执行！
    }





#else

    // =============== SDIO 1-bit 模式 ===============
    ESP_LOGI(TAG, "Initializing SD card in SDIO 1-bit mode");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT; // 强制 1-bit 模式

    sdmmc_slot_config_t slot_cfg = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_cfg.width = 1;
    slot_cfg.clk = SD_CLK_GPIO;
    slot_cfg.cmd = SD_CMD_GPIO;
    slot_cfg.d0 = SD_D0_GPIO;
    // D1/D2/D3 不使用（1-bit 模式）
    slot_cfg.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 3,
        .allocation_unit_size = 16 * 1024,
    };

    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_cfg, &mount_cfg, &sdcard);

#endif // SD_USE_SPI

  

    ESP_LOGI(TAG, "SD card mounted at %s", MOUNT_POINT);
    sdmmc_card_print_info(stdout, sdcard);
}




#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>


// #define MAX_FILES       100
// #define MAX_FILENAME    50


// 全局静态缓冲区（避免返回局部变量）
static char s_file_list[MAX_FILES][MAX_FILENAME];
static int s_file_count = 0;

/**
 * @brief 判断字符串是否以指定后缀结尾（不区分大小写）
 */
static bool ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix) {
        return false;
    }
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) {
        return false;
    }
    const char *p = str + str_len - suffix_len;
    const char *s = suffix;
    while (*p && *s) {
        if (tolower((unsigned char)*p) != tolower((unsigned char)*s)) {
            return false;
        }
        p++;
        s++;
    }
    return true;
}

/**
 * @brief 扫描 /sdcard 目录，筛选指定后缀的普通文件
 *
 * @param[in]  suffix   文件后缀，如 ".txt"（必须以 '.' 开头）
 *
 * @return 实际找到的文件数量（<= MAX_FILES）
 *
 * @note 结果存储在内部静态数组 s_file_list 中，
 *       可通过 get_filtered_files() 获取指针。
 */
int scan_files_by_extension(const char *suffix)
{
    if (!suffix || suffix[0] != '.') {
        ESP_LOGE(TAG, "Invalid suffix: must start with '.'");
        return 0;
    }

    DIR *dir = opendir(MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", MOUNT_POINT);
        return 0;
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir)) != NULL && count < MAX_FILES) {
        // 跳过 "." 和 ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 构造完整路径以判断是否为普通文件
        char full_path[265];
        snprintf(full_path, sizeof(full_path), "%s/%s", MOUNT_POINT, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) {
            ESP_LOGW(TAG, "Failed to stat %s, skipping", entry->d_name);
            continue;
        }

        // 仅处理普通文件（跳过目录、设备等）
        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        // 检查后缀
        if (ends_with(entry->d_name, suffix)) {
            // 安全复制，确保 null-terminated
            strncpy(s_file_list[count], entry->d_name, MAX_FILENAME - 1);
            s_file_list[count][MAX_FILENAME - 1] = '\0';
            count++;
        }
    }

    closedir(dir);
    s_file_count = count;
    ESP_LOGI(TAG, "Found %d file(s) with extension '%s'", count, suffix);
    return count;
}

/**
 * @brief 获取上一次扫描结果的文件列表指针
 *
 * @return 指向二维数组的指针（类型为 char (*)[MAX_FILENAME]）
 *         或 NULL（如果未调用 scan_files_by_extension）
 */
char (*get_filtered_files(void))[MAX_FILENAME]
{
    return (s_file_count > 0) ? s_file_list : NULL;
}

/**
 * @brief 获取上一次扫描到的文件数量
 */
int get_filtered_file_count(void)
{
    return s_file_count;
}