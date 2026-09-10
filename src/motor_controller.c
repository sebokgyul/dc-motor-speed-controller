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
    controller->configuration_valid = isfinite(kp) && isfinite(ki)
        && kp >= 0.0f && ki >= 0.0f;
}

ControllerOutput motor_controller_update(
    MotorController *controller,
    ControllerInput input,
    float sample_time_seconds
)
{
    ControllerOutput output = {0};
    float error;
    float proportional;
    float integral_candidate;
    float candidate_duty;
    bool can_integrate;
    bool target_is_valid = adc_value_is_valid(input.target_adc);
    bool speed_is_valid = adc_value_is_valid(input.speed_adc);

    if (!controller->configuration_valid
        || !isfinite(controller->kp)
        || !isfinite(controller->ki)
        || !isfinite(controller->integral)) {
        controller->integral = 0.0f;
        output.fault_led = true;
        output.status = CONTROLLER_CONFIGURATION_FAULT;
        output.fault_reason = CONTROLLER_FAULT_CONFIGURATION;
        return output;
    }

    if (target_is_valid) {
        output.target_rpm = adc_to_rpm(input.target_adc);
    }

    if (speed_is_valid) {
        output.measured_rpm = adc_to_rpm(input.speed_adc);
        output.measured_rpm_valid = true;
    }

    if (!target_is_valid || !speed_is_valid) {
        controller->integral = 0.0f;
        output.fault_led = true;
        output.status = CONTROLLER_INPUT_FAULT;
        output.fault_reason = target_is_valid
            ? CONTROLLER_FAULT_SPEED_ADC
            : CONTROLLER_FAULT_TARGET_ADC;
        return output;
    }

    if (!isfinite(sample_time_seconds) || sample_time_seconds <= 0.0f) {
        controller->integral = 0.0f;
        output.fault_led = true;
        output.status = CONTROLLER_INPUT_FAULT;
        output.fault_reason = CONTROLLER_FAULT_SAMPLE_TIME;
        return output;
    }

    if (!input.enable) {
        controller->integral = 0.0f;
        output.status = CONTROLLER_DISABLED;
        return output;
    }

    error = output.target_rpm - output.measured_rpm;
    proportional = controller->kp * error;
    integral_candidate = controller->integral
        + controller->ki * error * sample_time_seconds;
    candidate_duty = proportional + integral_candidate;

    if (!isfinite(proportional)
        || !isfinite(integral_candidate)
        || !isfinite(candidate_duty)) {
        controller->integral = 0.0f;
        output.fault_led = true;
        output.status = CONTROLLER_INPUT_FAULT;
        output.fault_reason = CONTROLLER_FAULT_NUMERIC;
        return output;
    }

    can_integrate = (candidate_duty >= PWM_MIN_DUTY && candidate_duty <= PWM_MAX_DUTY)
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
        case CONTROLLER_CONFIGURATION_FAULT:
            return "configuration fault";
        default:
            return "unknown";
    }
}
