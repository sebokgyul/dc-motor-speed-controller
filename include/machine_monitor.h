#ifndef MACHINE_MONITOR_H
#define MACHINE_MONITOR_H

#include <stdbool.h>

#include "motor_controller.h"

typedef enum {
    MACHINE_DISABLED = 0,
    MACHINE_RUNNING,
    MACHINE_WARNING,
    MACHINE_FAULT
} MachineStatus;

typedef enum {
    FAULT_NONE = 0,
    FAULT_SENSOR_SIGNAL_INVALID,
    FAULT_MOTOR_STALL,
    FAULT_CONTROLLER_INPUT_INVALID,
    FAULT_CONTROLLER_CONFIGURATION
} FaultCode;

typedef struct {
    MachineStatus status;
    FaultCode fault_code;
    float stall_elapsed_seconds;
    bool stall_active;
    bool stop_requested;
} MachineMonitor;

void machine_monitor_init(MachineMonitor *monitor);
MachineStatus machine_monitor_update(
    MachineMonitor *monitor,
    ControllerOutput controller_output,
    bool run_requested,
    float sample_time_seconds
);
bool machine_monitor_stop_requested(const MachineMonitor *monitor);
const char *machine_status_name(MachineStatus status);
const char *fault_code_name(FaultCode fault_code);
const char *fault_message(FaultCode fault_code);

#endif
