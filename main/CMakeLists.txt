idf_component_register(SRCS "main.c" 
                           
                 
                            "ui/screens.c"
                            "ui/styles.c"
                            "ui/images.c"
                            "ui/ui.c"
                            "ui/eez-flow.cpp"
                            "ui/actions.c"
                     
                            "lcd/lcd.c"
                            "ns4168/audio.c"
                            "sd/sd.c"
                            "ui/vars.c"
                           "ui/ui_font_cn_font.c"
                           "ui/ui_font_cn_font_20.c"
                           "mytimer/mytimer.c"
                           "record/record.c"
                           "i2c_sensor/qmi.c"
                                 "i2c_sensor/aht20.c"
                                 "i2c_sensor/draw_cube.c"
                                   "wifi_time/wifi_time.c"
                                   "bt/bt.c"

                            
                    INCLUDE_DIRS "."
                                "ns4168"
                               
                                 "ui"
                               
                                 "lcd"
                                "sd"
                                "mytimer"
                                "record"
                                "i2c_sensor"
                                "wifi_time"
"bt"

                     REQUIRES fatfs  esp_driver_i2s esp_wifi nvs_flash bt esp_http_server
                             EMBED_FILES "wifi.html"
                                 )