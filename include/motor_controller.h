#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <stdbool.h>

#include "motor_config.h"

typedef enum {
    CONTROLLER_DISABLED = 0,
    CONTROLLER_RUNNING,
    CONTROLLER_INPUT_FAULT
} ControllerStatus;

typedef struct {
    int target_adc;
    int speed_adc;
    bool enable;
} ControllerInput;

typedef struct {
    float target_rpm;
    float measured_rpm;
    float pwm_duty;
    bool fault_led;
    ControllerStatus status;
} ControllerOutput;

typedef struct {
    float kp;
    float ki;
    float integral;
} MotorController;

void motor_controller_init(MotorController *controller, float kp, float ki);
ControllerOutput motor_controller_update(
    MotorController *controller,
    ControllerInput input,
    float sample_time_seconds
);
const char *controller_status_name(ControllerStatus status);

#endif
