#pragma once 


#include "esp_err.h"
#include "driver/i2c_master.h"
#include "lvgl.h"



esp_err_t qmi8658_init(i2c_master_bus_handle_t bus) ; 
void     qmi8658_start_task(); 




