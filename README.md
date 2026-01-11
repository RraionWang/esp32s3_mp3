# readme


## 说明
这是一个驱动文件，专门用于驱动172*320的触摸屏幕，调用方式参照main中的写法


## 引脚定义

|GPIO|LCD|SD卡|麦克风|NS4168|QMI8658A|AHT20|BMP280|电池|
|-|-|-|-|-|-|-|-|-|
|13|CTP_RST||||||||
|12|CTP_INT||||||||
|11|CTP_SDA||||I2C_SDA|I2C_SDA|I2C_SDA||
|10|CTP_SCL||||I2C_SCL|I2C_SCL|I2C_SCL||
|9|LCD_SCL||||||||
|46|LCD_RST||||||||
|3|LCD_CS||||||||
|17|LCD_SDA||||||||
|18|LCD_RS||||||||
|8|LCD_BLK||||||||
|41|||||||CSB||
|42|||||||SDO||
|48||SD_CS|||||||
|47||SD_MOSI|||||||
|21||SD_CLK|||||||
|14||SD_MISO|||||||
|35|||MIC_WS||||||
|36|||MIC_SD||||||
|45|||MIC_SCK||||||
|7||||NS_CTRL|||||
|6||||NS_LRCLK|||||
|5||||NS_BCLK|||||
|4||||NS_SDATA|||||
|37||||||||BAT_ADC|

## 音频处理

ffmpeg -i qingtian.wav -ac 1 -ar 44100 -sample_fmt s16 -f wav qingtian_mono.wav

## eez 兼容

设置lvgl 版本为9.2.0就可以不用修改错误，如   lvgl/lvgl: 9.2.0

## 字体问题

```sh
Component config
 └─ FAT Filesystem support
     └─ Long filename support
         ├─ [*] Enable Long File Name (LFN)
         ├─ LFN encoding → UTF-8
         └─ Code page → 65001 (UTF-8)
```


