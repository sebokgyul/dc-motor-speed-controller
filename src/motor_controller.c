#include "motor_controller.h"

#include <math.h>

#define PWM_MIN_DUTY 0.0f
#define PWM_MAX_DUTY 100.0f

static float clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
}

static bool adc_value_is_valid(int value)
{
    return value >= 0 && value <= ADC_MAX_VALUE;
}

static float adc_to_rpm(int adc_value)
{
    return ((float)adc_value / (float)ADC_MAX_VALUE) * MOTOR_MAX_SPEED_RPM;
}

void motor_controller_init(MotorController *controller, float kp, float ki)
{
    controller->kp = kp;
    controller->ki = ki;
    controller->integral = 0.0f;
}

ControllerOutput motor_controller_update(
    MotorController *controller,
    ControllerInput input,
    float sample_time_seconds
)
{
    ControllerOutput output = {0};
    float error;
    float integral_candidate;
    float candidate_duty;
    bool can_integrate;

    if (!adc_value_is_valid(input.target_adc)
        || !adc_value_is_valid(input.speed_adc)
        || !isfinite(sample_time_seconds)
        || sample_time_seconds <= 0.0f) {
        controller->integral = 0.0f;
        output.fault_led = true;
        output.status = CONTROLLER_INPUT_FAULT;
        return output;
    }

    output.target_rpm = adc_to_rpm(input.target_adc);
    output.measured_rpm = adc_to_rpm(input.speed_adc);

    if (!input.enable) {
        controller->integral = 0.0f;
        output.status = CONTROLLER_DISABLED;
        return output;
    }

    error = output.target_rpm - output.measured_rpm;
    integral_candidate = controller->integral
        + controller->ki * error * sample_time_seconds;
    candidate_duty = controller->kp * error + integral_candidate;

    can_integrate = (candidate_duty > PWM_MIN_DUTY && candidate_duty < PWM_MAX_DUTY)
        || (candidate_duty >= PWM_MAX_DUTY && error < 0.0f)
        || (candidate_duty <= PWM_MIN_DUTY && error > 0.0f);

    if (can_integrate) {
        controller->integral = integral_candidate;
    }

    output.pwm_duty = clamp(candidate_duty, PWM_MIN_DUTY, PWM_MAX_DUTY);
    output.status = CONTROLLER_RUNNING;
    return output;
}

const char *controller_status_name(ControllerStatus status)
{
    switch (status) {
        case CONTROLLER_DISABLED:
            return "disabled";
        case CONTROLLER_RUNNING:
            return "running";
        case CONTROLLER_INPUT_FAULT:
            return "input fault";
        default:
            return "unknown";
    }
}
