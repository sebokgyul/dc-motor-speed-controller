#ifndef MOTOR_MODEL_H
#define MOTOR_MODEL_H

typedef struct {
    float speed_rpm;
    float time_constant_seconds;
} MotorModel;

void motor_model_init(MotorModel *model, float time_constant_seconds);
void motor_model_update(MotorModel *model, float pwm_duty, float sample_time_seconds);
int motor_model_speed_adc(const MotorModel *model);
int motor_model_rpm_to_adc(float speed_rpm);

#endif
