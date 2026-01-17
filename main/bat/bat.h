#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void adc_gpio1_init(void);
void adc_gpio1_task(void *arg);
void chart_voltage_init(void);
void chart_voltage_task(void *arg);
void charge_state_init(void);
void charge_state_task(void *arg);


#ifdef __cplusplus
}
#endif
