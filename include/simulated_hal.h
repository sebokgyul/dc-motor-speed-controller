#ifndef SIMULATED_HAL_H
#define SIMULATED_HAL_H

#include <stdbool.h>

typedef enum {
    ADC_TARGET_SPEED = 0,
    ADC_MEASURED_SPEED
} AdcChannel;

typedef struct {
    int target_adc;
    int speed_adc;
    bool enable_input;
    float pwm_duty;
    bool fault_led;
} SimulatedHal;

void hal_init(SimulatedHal *hal);
void hal_set_inputs(SimulatedHal *hal, int target_adc, int speed_adc, bool enable);
int hal_adc_read(const SimulatedHal *hal, AdcChannel channel);
bool hal_gpio_read_enable(const SimulatedHal *hal);
void hal_pwm_write(SimulatedHal *hal, float duty);
void hal_gpio_write_fault(SimulatedHal *hal, bool active);
float hal_pwm_read(const SimulatedHal *hal);
bool hal_gpio_read_fault(const SimulatedHal *hal);

#endif
