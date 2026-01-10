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

                            
                    INCLUDE_DIRS "."
                                "ns4168"
                               
                                 "ui"
                               
                                 "lcd"
                                "sd"
                                "mytimer"


                     REQUIRES fatfs  esp_driver_i2s
                                 )