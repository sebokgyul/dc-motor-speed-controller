#include "simulated_hal.h"

#include <math.h>

#define PWM_MIN_DUTY 0.0f
#define PWM_MAX_DUTY 100.0f

void hal_init(SimulatedHal *hal)
{
    hal->target_adc = 0;
    hal->speed_adc = 0;
    hal->enable_input = false;
    hal->pwm_duty = 0.0f;
    hal->fault_led = false;
}

void hal_set_inputs(SimulatedHal *hal, int target_adc, int speed_adc, bool enable)
{
    hal->target_adc = target_adc;
    hal->speed_adc = speed_adc;
    hal->enable_input = enable;
}

int hal_adc_read(const SimulatedHal *hal, AdcChannel channel)
{
    if (channel == ADC_TARGET_SPEED) {
        return hal->target_adc;
    }

    if (channel == ADC_MEASURED_SPEED) {
        return hal->speed_adc;
    }

    return -1;
}

bool hal_gpio_read_enable(const SimulatedHal *hal)
{
    return hal->enable_input;
}

void hal_pwm_write(SimulatedHal *hal, float duty)
{
    if (!isfinite(duty) || duty <= PWM_MIN_DUTY) {
        hal->pwm_duty = PWM_MIN_DUTY;
    } else if (duty >= PWM_MAX_DUTY) {
        hal->pwm_duty = PWM_MAX_DUTY;
    } else {
        hal->pwm_duty = duty;
    }
}

void hal_gpio_write_fault(SimulatedHal *hal, bool active)
{
    hal->fault_led = active;
}

float hal_pwm_read(const SimulatedHal *hal)
{
    return hal->pwm_duty;
}

bool hal_gpio_read_fault(const SimulatedHal *hal)
{
    return hal->fault_led;
}
