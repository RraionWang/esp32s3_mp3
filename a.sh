
Saved PC:0x400556d2
--- 0x400556d2: strlen in ROM
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce2820,len:0x159c
load:0x403c8700,len:0xd24
load:0x403cb700,len:0x2f48
entry 0x403c8924
I (24) boot: ESP-IDF v5.5 2nd stage bootloader
I (24) boot: compile time Jan 13 2026 02:42:28
I (25) boot: Multicore bootloader
I (25) boot: chip revision: v0.2
I (27) boot: efuse block revision: v1.3
I (31) boot.esp32s3: Boot SPI Speed : 80MHz
I (35) boot.esp32s3: SPI Mode       : DIO
I (39) boot.esp32s3: SPI Flash Size : 16MB
I (42) boot: Enabling RNG early entropy source...
I (47) boot: Partition Table:
I (50) boot: ## Label            Usage          Type ST Offset   Length
I (56) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (62) boot:  1 factory          factory app      00 00 00010000 00700000
I (69) boot:  2 spiffs           Unknown data     01 82 00710000 00800000
I (75) boot: End of partition table
I (79) esp_image: segment 0: paddr=00010020 vaddr=3c110020 size=59000h (364544) map
I (150) esp_image: segment 1: paddr=00069028 vaddr=3fc9f600 size=05cb0h ( 23728) load
I (156) esp_image: segment 2: paddr=0006ece0 vaddr=40374000 size=01338h (  4920) load
I (157) esp_image: segment 3: paddr=00070020 vaddr=42000020 size=10ebe0h (1108960) map
I (357) esp_image: segment 4: paddr=0017ec08 vaddr=40375338 size=1a1c0h (106944) load
I (381) esp_image: segment 5: paddr=00198dd0 vaddr=600fe000 size=00020h (    32) load
I (392) boot: Loaded app from partition at offset 0x10000
I (392) boot: Disabling RNG early entropy source...
I (403) octal_psram: vendor id    : 0x0d (AP)
I (403) octal_psram: dev id       : 0x02 (generation 3)
I (403) octal_psram: density      : 0x03 (64 Mbit)
I (405) octal_psram: good-die     : 0x01 (Pass)
I (409) octal_psram: Latency      : 0x01 (Fixed)
I (414) octal_psram: VCC          : 0x01 (3V)
I (418) octal_psram: SRF          : 0x01 (Fast Refresh)
I (423) octal_psram: BurstType    : 0x01 (Hybrid Wrap)
I (428) octal_psram: BurstLen     : 0x01 (32 Byte)
I (432) octal_psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (437) octal_psram: DriveStrength: 0x00 (1/1)
I (442) MSPI Timing: PSRAM timing tuning index: 5
I (446) esp_psram: Found 8MB PSRAM device
I (450) esp_psram: Speed: 80MHz
I (453) cpu_start: Multicore app
I (888) esp_psram: SPI SRAM memory test OK
I (897) cpu_start: Pro cpu start user code
I (897) cpu_start: cpu freq: 160000000 Hz
I (897) app_init: Application information:
I (897) app_init: Project name:     lvgl_esp32
I (901) app_init: App version:      a045bb1-dirty
I (906) app_init: Compile time:     Jan 13 2026 11:06:07
I (911) app_init: ELF file SHA256:  9c60ea838...
I (915) app_init: ESP-IDF:          v5.5
I (919) efuse_init: Min chip rev:     v0.0
I (923) efuse_init: Max chip rev:     v0.99
I (927) efuse_init: Chip rev:         v0.2
I (930) heap_init: Initializing. RAM available for dynamic allocation:
I (937) heap_init: At 3FCC2028 len 000276E8 (157 KiB): RAM
I (942) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
I (947) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (952) heap_init: At 600FE020 len 00001FC8 (7 KiB): RTCRAM
I (958) esp_psram: Adding pool of 8192K of PSRAM memory to heap allocator
I (965) spi_flash: detected chip: generic
I (968) spi_flash: flash io: dio
I (971) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (977) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (984) coexist: coex firmware version: 831ec70
I (988) coexist: coexist rom version e7ae62f
I (992) main_task: Started on CPU0
I (1002) main_task: Calling app_main()
I (1002) ram:
--- 内存状态报告 ---

I (1002) ram: 内部 RAM 剩余: 192 KB

I (1002) ram: 外部 PSRAM 剩余: 8189 KB

I (1012) ram: 历史最低水位线: 8381 KB

I (1012) ram: --------------------

I (1022) wifi_time: wifi_time_sys_init
I (1062) pp: pp rom version: e7ae62f
I (1062) net80211: net80211 rom version: e7ae62f
I (1072) wifi:wifi driver task: 3fcee830, prio:23, stack:6656, core=0
I (1092) wifi:wifi firmware version: f3dbad7
I (1092) wifi:wifi certification version: v7.0
I (1092) wifi:config NVS flash: enabled
I (1092) wifi:config nano formatting: disabled
I (1092) wifi:Init data frame dynamic rx buffer num: 10
I (1102) wifi:Init static rx mgmt buffer num: 5
I (1102) wifi:Init management short buffer num: 32
I (1112) wifi:Init static tx buffer num: 16
I (1112) wifi:Init tx cache buffer num: 32
I (1112) wifi:Init static tx FG buffer num: 2
I (1122) wifi:Init static rx buffer size: 1600
I (1122) wifi:Init static rx buffer num: 10
I (1132) wifi:Init dynamic rx buffer num: 10
I (1132) wifi_init: rx ba win: 6
I (1132) wifi_init: accept mbox: 6
I (1142) wifi_init: tcpip mbox: 32
I (1142) wifi_init: udp mbox: 6
I (1142) wifi_init: tcp mbox: 6
I (1142) wifi_init: tcp tx win: 5760
I (1152) wifi_init: tcp rx win: 5760
I (1152) wifi_init: tcp mss: 1440
I (1152) wifi_init: WiFi/LWIP prefer SPIRAM
I (1162) wifi_init: WiFi IRAM OP enabled
I (1162) wifi_init: WiFi RX IRAM OP enabled
I (1172) phy_init: phy_version 701,f4f1da3a,Mar  3 2025,15:50:10
I (1222) wifi:mode : sta (90:70:69:0b:59:6c) + softAP (90:70:69:0b:59:6d)
I (1222) wifi:enable tsf
I (1222) wifi:Total power save buffer number: 8
I (1232) wifi:Init max length of beacon: 752/752
I (1232) wifi:Init max length of beacon: 752/752
I (1232) wifi_time: WEB_PROV: SoftAP started: ESP32S3
I (1232) wifi_time: WIFI_EVENT_STA_START
I (1242) wifi_time: WEB_PROV: IP: 192.168.4.1
I (1242) esp_netif_lwip: DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1
I (1242) sdspi_transaction: cmd=52, R1 response: command CRC error
I (1262) wifi_time: WEB_PROV: HTTP server started
E (1262) sdmmc_io: sdmmc_io_reset: unexpected return: 0x109
E (1272) vfs_fat_sdmmc: sdmmc_card_init failed (0x109).
E (1272) sd: Failed to mount SD card in SPI mode: ESP_ERR_INVALID_CRC
I (1402) LVGL: Starting LVGL task
I (1452) LVGL: LVGL 输入设备注册完成
E (1512) i2c.master: I2C transaction unexpected nack detected
E (1512) i2c.master: s_i2c_synchronous_transaction(945): I2C transaction failed
E (1512) i2c.master: i2c_master_transmit_receive(1248): I2C transaction failed
I (1552) LVGL: CST816D ID: 0xB6
I (1552) LVGL: ✅ CST816D 初始化完成
I (1552) AHT20: ✅ AHT20 初始化完成（共享 I2C 总线）
I (1552) QMI8658: QMI8658 found at 0x6A, WHO_AM_I=0x05 (hint=0x05)
I (1562) QMI8658: ✅ QMI8658 init OK (shared I2C)
E (1802) sd: Failed to open directory: /sdcard
W (1802) main: No .wav files found on SD card
E (1802) main: Failed to init wav player
E (1802) sd: Failed to open directory: /sdcard
W (1802) main: No .wav files found.
I (1852) AHT20: AHT20 task started
I (1852) main_task: Returned from app_main()
I (2012) VOL: 设置音量为0.000000
I (2022) ram:
--- 内存状态报告 ---

I (2022) ram: 内部 RAM 剩余: 56 KB

I (2022) ram: 外部 PSRAM 剩余: 8067 KB

I (2022) ram: 历史最低水位线: 8097 KB

I (2022) ram: --------------------

I (2812) RTC: 2026-01-13 11:06:59
I (3022) ram: 
--- 内存状态报告 ---

I (3022) ram: 内部 RAM 剩余: 56 KB

I (3022) ram: 外部 PSRAM 剩余: 8067 KB

I (3022) ram: 历史最低水位线: 8097 KB

I (3022) ram: --------------------

I (3812) RTC: 2026-01-13 11:07:00
I (4022) ram: 
--- 内存状态报告 ---

I (4022) ram: 内部 RAM 剩余: 56 KB

I (4022) ram: 外部 PSRAM 剩余: 8067 KB

I (4022) ram: 历史最低水位线: 8097 KB

I (4022) ram: --------------------

I (4822) RTC: 2026-01-13 11:07:01
I (5022) ram: 
--- 内存状态报告 ---

I (5022) ram: 内部 RAM 剩余: 56 KB

I (5022) ram: 外部 PSRAM 剩余: 8067 KB

I (5022) ram: 历史最低水位线: 8097 KB

I (5022) ram: --------------------

I (5212) LVGL: x=51,y=174
I (5262) LVGL: x=51,y=174
I (5832) RTC: 2026-01-13 11:07:02
I (6022) ram: 
--- 内存状态报告 ---

I (6022) ram: 内部 RAM 剩余: 56 KB

I (6022) ram: 外部 PSRAM 剩余: 8067 KB

I (6022) ram: 历史最低水位线: 8097 KB

I (6022) ram: --------------------

I (6462) LVGL: x=47,y=178
I (6512) LVGL: x=47,y=178
I (6832) RTC: 2026-01-13 11:07:03
I (7022) ram: 
--- 内存状态报告 ---

I (7022) ram: 内部 RAM 剩余: 56 KB

I (7022) ram: 外部 PSRAM 剩余: 8067 KB

I (7022) ram: 历史最低水位线: 8097 KB

I (7022) ram: --------------------

I (7352) LVGL: x=70,y=187
I (7392) LVGL: x=70,y=187
I (7832) RTC: 2026-01-13 11:07:04
I (8022) ram: 
--- 内存状态报告 ---

I (8022) ram: 内部 RAM 剩余: 56 KB

I (8022) ram: 外部 PSRAM 剩余: 8067 KB

I (8022) ram: 历史最低水位线: 8097 KB

I (8022) ram: --------------------

I (8212) LVGL: x=59,y=184
I (8262) LVGL: x=59,y=184
I (8832) RTC: 2026-01-13 11:07:05
I (9022) ram: 
--- 内存状态报告 ---

I (9022) ram: 内部 RAM 剩余: 56 KB

I (9022) ram: 外部 PSRAM 剩余: 8067 KB

I (9022) ram: 历史最低水位线: 8097 KB

I (9022) ram: --------------------

I (9832) RTC: 2026-01-13 11:07:06
I (10022) ram: 
--- 内存状态报告 ---

I (10022) ram: 内部 RAM 剩余: 56 KB

I (10022) ram: 外部 PSRAM 剩余: 8067 KB

I (10022) ram: 历史最低水位线: 8097 KB

I (10022) ram: --------------------

I (10832) RTC: 2026-01-13 11:07:07
I (11022) ram: 
--- 内存状态报告 ---

I (11022) ram: 内部 RAM 剩余: 56 KB

I (11022) ram: 外部 PSRAM 剩余: 8067 KB

I (11022) ram: 历史最低水位线: 8097 KB

I (11022) ram: --------------------

--- To exit from IDF monitor please use "Ctrl+]". Alternatively, you can use Ctrl+T Ctrl+X to exit.
I (11832) RTC: 2026-01-13 11:07:08
I (12022) ram: 
--- 内存状态报告 ---

I (12022) ram: 内部 RAM 剩余: 56 KB

I (12022) ram: 外部 PSRAM 剩余: 8067 KB

I (12022) ram: 历史最低水位线: 8097 KB

I (12022) ram: --------------------

I (12832) RTC: 2026-01-13 11:07:09
I (13022) ram: 
--- 内存状态报告 ---

I (13022) ram: 内部 RAM 剩余: 56 KB

I (13022) ram: 外部 PSRAM 剩余: 8067 KB

I (13022) ram: 历史最低水位线: 8097 KB

I (13022) ram: --------------------

I (13132) LVGL: x=43,y=195
I (13172) LVGL: x=43,y=195
I (13842) RTC: 2026-01-13 11:07:10
I (14022) ram: 
--- 内存状态报告 ---

I (14022) ram: 内部 RAM 剩余: 54 KB

I (14022) ram: 外部 PSRAM 剩余: 8067 KB

I (14022) ram: 历史最低水位线: 8097 KB

I (14022) ram: --------------------

I (14852) RTC: 2026-01-13 11:07:11
I (15022) ram: 
--- 内存状态报告 ---

I (15022) ram: 内部 RAM 剩余: 54 KB

I (15022) ram: 外部 PSRAM 剩余: 8067 KB

I (15022) ram: 历史最低水位线: 8097 KB

I (15022) ram: --------------------

I (15852) RTC: 2026-01-13 11:07:12
I (16022) ram: 
--- 内存状态报告 ---

I (16022) ram: 内部 RAM 剩余: 55 KB

I (16022) ram: 外部 PSRAM 剩余: 8067 KB

I (16022) ram: 历史最低水位线: 8097 KB

I (16022) ram: --------------------

I (16852) RTC: 2026-01-13 11:07:13
I (17022) ram: 
--- 内存状态报告 ---

I (17022) ram: 内部 RAM 剩余: 55 KB

I (17022) ram: 外部 PSRAM 剩余: 8067 KB

I (17022) ram: 历史最低水位线: 8097 KB

I (17022) ram: --------------------

I (17852) RTC: 2026-01-13 11:07:14
I (18022) ram: 
--- 内存状态报告 ---

I (18022) ram: 内部 RAM 剩余: 54 KB

I (18022) ram: 外部 PSRAM 剩余: 8067 KB

I (18022) ram: 历史最低水位线: 8097 KB

I (17022) ram: 外部 PSRAM 剩余: 8067 KB

I (17022) ram: 历史最低水位线: 8097 KB

I (17022) ram: --------------------

I (17852) RTC: 2026-01-13 11:07:14
I (18022) ram:
--- 内存状态报告 ---

I (18022) ram: 内部 RAM 剩余: 54 KB

I (18022) ram: 外部 PSRAM 剩余: 8067 KB

I (18022) ram: 历史最低水位线: 8097 KB
--- 内存状态报告 ---

I (18022) ram: 内部 RAM 剩余: 54 KB

I (18022) ram: 外部 PSRAM 剩余: 8067 KB

I (18022) ram: 历史最低水位线: 8097 KB

I (18022) ram: 外部 PSRAM 剩余: 8067 KB

I (18022) ram: 历史最低水位线: 8097 KB
I (18022) ram: 历史最低水位线: 8097 KB

I (18022) ram: --------------------

I (18852) RTC: 2026-01-13 11:07:15
I (19022) ram:
--- 内存状态报告 ---

I (19022) ram: 内部 RAM 剩余: 54 KB

I (19022) ram: 外部 PSRAM 剩余: 8067 KB

I (19022) ram: 历史最低水位线: 8097 KB

I (19022) ram: --------------------

I (19852) RTC: 2026-01-13 11:07:16
I (20022) ram: 
--- 内存状态报告 ---

I (20022) ram: 内部 RAM 剩余: 54 KB

I (20022) ram: 外部 PSRAM 剩余: 8067 KB

I (20022) ram: 历史最低水位线: 8097 KB

I (20022) ram: --------------------

I (20852) RTC: 2026-01-13 11:07:17
I (21022) ram: 
--- 内存状态报告 ---

I (21022) ram: 内部 RAM 剩余: 54 KB

I (21022) ram: 外部 PSRAM 剩余: 8067 KB

I (21022) ram: 历史最低水位线: 8097 KB

I (21022) ram: --------------------

I (21852) RTC: 2026-01-13 11:07:18
I (22022) ram: 
--- 内存状态报告 ---

I (22022) ram: 内部 RAM 剩余: 54 KB

I (22022) ram: 外部 PSRAM 剩余: 8067 KB

I (22022) ram: 历史最低水位线: 8097 KB

I (23022) ram:
--- 内存状态报告 ---

I (23022) ram: 内部 RAM 剩余: 54 KB

I (23022) ram: 外部 PSRAM 剩余: 8067 KB

I (23022) ram: 历史最低水位线: 8097 KB

I (23022) ram: --------------------

I (23852) RTC: 2026-01-13 11:07:20