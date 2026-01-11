#pragma once 

#include "esp_err.h"
#include "driver/i2c_master.h"

esp_err_t aht20_init(i2c_master_bus_handle_t bus) ;
void aht20_start_task(void) ; 

