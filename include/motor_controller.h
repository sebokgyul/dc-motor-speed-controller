#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <stdbool.h>

#include "motor_config.h"

typedef enum {
    CONTROLLER_DISABLED = 0,
    CONTROLLER_RUNNING,
    CONTROLLER_INPUT_FAULT,
    CONTROLLER_CONFIGURATION_FAULT
} ControllerStatus;

typedef enum {
    CONTROLLER_FAULT_NONE = 0,
    CONTROLLER_FAULT_TARGET_ADC,
    CONTROLLER_FAULT_SPEED_ADC,
    CONTROLLER_FAULT_SAMPLE_TIME,
    CONTROLLER_FAULT_NUMERIC,
    CONTROLLER_FAULT_CONFIGURATION
} ControllerFaultReason;

typedef struct {
    int target_adc;
    int speed_adc;
    bool enable;
} ControllerInput;

typedef struct {
    float target_rpm;
    float measured_rpm;
    bool measured_rpm_valid;
    float pwm_duty;
    bool fault_led;
    ControllerStatus status;
    ControllerFaultReason fault_reason;
} ControllerOutput;

typedef struct {
    float kp;
    float ki;
    float integral;
    bool configuration_valid;
} MotorController;

void motor_controller_init(MotorController *controller, float kp, float ki);
ControllerOutput motor_controller_update(
    MotorController *controller,
    ControllerInput input,
    float sample_time_seconds
);
const char *controller_status_name(ControllerStatus status);

#endif
