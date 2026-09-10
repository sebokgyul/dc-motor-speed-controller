#include "machine_monitor.h"

#include <math.h>

#define STALL_MINIMUM_TARGET_RPM 1000.0f
#define STALL_MAXIMUM_SPEED_RATIO 0.25f
#define STALL_MINIMUM_PWM_DUTY 70.0f
#define STALL_CONFIRMATION_SECONDS 0.75f
#define STALL_TIME_TOLERANCE_SECONDS 0.000001f

static void latch_fault(MachineMonitor *monitor, FaultCode fault_code)
{
    monitor->status = MACHINE_FAULT;
    monitor->fault_code = fault_code;
    monitor->stop_requested = true;
}

void machine_monitor_init(MachineMonitor *monitor)
{
    monitor->status = MACHINE_DISABLED;
    monitor->fault_code = FAULT_NONE;
    monitor->stall_elapsed_seconds = 0.0f;
    monitor->stop_requested = false;
}

MachineStatus machine_monitor_update(
    MachineMonitor *monitor,
    ControllerOutput controller_output,
    bool run_requested,
    float sample_time_seconds
)
{
    bool possible_stall;

    if (monitor->fault_code != FAULT_NONE) {
        monitor->status = MACHINE_FAULT;
        return monitor->status;
    }

    if (controller_output.status == CONTROLLER_INPUT_FAULT) {
        latch_fault(
            monitor,
            controller_output.fault_reason == CONTROLLER_FAULT_SPEED_ADC
                ? FAULT_SENSOR_SIGNAL_INVALID
                : FAULT_CONTROLLER_INPUT_INVALID
        );
        return monitor->status;
    }

    if (controller_output.status == CONTROLLER_CONFIGURATION_FAULT
        || !isfinite(controller_output.target_rpm)
        || !isfinite(controller_output.measured_rpm)
        || !isfinite(controller_output.pwm_duty)
        || !isfinite(sample_time_seconds)
        || sample_time_seconds <= 0.0f) {
        latch_fault(monitor, FAULT_CONTROLLER_CONFIGURATION);
        return monitor->status;
    }

    if (controller_output.status != CONTROLLER_RUNNING
        && controller_output.status != CONTROLLER_DISABLED) {
        latch_fault(monitor, FAULT_CONTROLLER_CONFIGURATION);
        return monitor->status;
    }

    if (!run_requested || controller_output.status == CONTROLLER_DISABLED) {
        monitor->stall_elapsed_seconds = 0.0f;
        monitor->status = MACHINE_DISABLED;
        return monitor->status;
    }

    possible_stall = controller_output.target_rpm >= STALL_MINIMUM_TARGET_RPM
        && controller_output.measured_rpm
            <= controller_output.target_rpm * STALL_MAXIMUM_SPEED_RATIO
        && controller_output.pwm_duty >= STALL_MINIMUM_PWM_DUTY;

    if (!possible_stall) {
        monitor->stall_elapsed_seconds = 0.0f;
        monitor->status = MACHINE_RUNNING;
        return monitor->status;
    }

    monitor->stall_elapsed_seconds += sample_time_seconds;
    if (monitor->stall_elapsed_seconds + STALL_TIME_TOLERANCE_SECONDS
        >= STALL_CONFIRMATION_SECONDS) {
        latch_fault(monitor, FAULT_MOTOR_STALL);
    } else {
        monitor->status = MACHINE_WARNING;
    }

    return monitor->status;
}

bool machine_monitor_stop_requested(const MachineMonitor *monitor)
{
    return monitor->stop_requested;
}

const char *machine_status_name(MachineStatus status)
{
    switch (status) {
        case MACHINE_DISABLED:
            return "disabled";
        case MACHINE_RUNNING:
            return "running";
        case MACHINE_WARNING:
            return "warning";
        case MACHINE_FAULT:
            return "fault";
        default:
            return "unknown";
    }
}

const char *fault_code_name(FaultCode fault_code)
{
    switch (fault_code) {
        case FAULT_NONE:
            return "NONE";
        case FAULT_SENSOR_SIGNAL_INVALID:
            return "F101_SENSOR_SIGNAL_INVALID";
        case FAULT_MOTOR_STALL:
            return "F201_MOTOR_STALL";
        case FAULT_CONTROLLER_INPUT_INVALID:
            return "F902_CONTROLLER_INPUT_INVALID";
        case FAULT_CONTROLLER_CONFIGURATION:
            return "F901_CONTROLLER_CONFIGURATION";
        default:
            return "UNKNOWN";
    }
}

const char *fault_message(FaultCode fault_code)
{
    switch (fault_code) {
        case FAULT_NONE:
            return "No active fault.";
        case FAULT_SENSOR_SIGNAL_INVALID:
            return "Measured-speed signal is outside the valid ADC range.";
        case FAULT_MOTOR_STALL:
            return "Motor speed remained below 25% of target during sustained high PWM.";
        case FAULT_CONTROLLER_INPUT_INVALID:
            return "Controller received invalid target or timing data.";
        case FAULT_CONTROLLER_CONFIGURATION:
            return "Controller or monitor received an invalid numeric configuration.";
        default:
            return "Unknown fault.";
    }
}
