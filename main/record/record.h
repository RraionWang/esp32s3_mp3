// record.h
#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

  void  init_mic_debug(void);


    
void record_to_sd_task(void *args);


#ifdef __cplusplus
}
#endif
