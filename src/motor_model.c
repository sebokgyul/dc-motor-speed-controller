#include "motor_model.h"

#include <math.h>

#include "motor_config.h"

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

void motor_model_init(MotorModel *model, float time_constant_seconds)
{
    model->speed_rpm = 0.0f;
    model->time_constant_seconds = time_constant_seconds;
}

void motor_model_update(MotorModel *model, float pwm_duty, float sample_time_seconds)
{
    float limited_duty;
    float steady_state_speed;

    if (!isfinite(model->time_constant_seconds)
        || !isfinite(sample_time_seconds)
        || model->time_constant_seconds <= 0.0f
        || sample_time_seconds <= 0.0f) {
        return;
    }

    limited_duty = clamp(pwm_duty, 0.0f, PWM_MAX_DUTY);
    steady_state_speed = MOTOR_MAX_SPEED_RPM * limited_duty / PWM_MAX_DUTY;

    model->speed_rpm += sample_time_seconds / model->time_constant_seconds
        * (steady_state_speed - model->speed_rpm);
    model->speed_rpm = clamp(model->speed_rpm, 0.0f, MOTOR_MAX_SPEED_RPM);
}

int motor_model_speed_adc(const MotorModel *model)
{
    return motor_model_rpm_to_adc(model->speed_rpm);
}

int motor_model_rpm_to_adc(float speed_rpm)
{
    float limited_speed = clamp(speed_rpm, 0.0f, MOTOR_MAX_SPEED_RPM);
    float adc_value = limited_speed / MOTOR_MAX_SPEED_RPM * (float)ADC_MAX_VALUE;

    return (int)(adc_value + 0.5f);
}
