#ifndef SIMULATION_H
#define SIMULATION_H

#include <stdbool.h>
#include <stdint.h>

#include "machine_monitor.h"
#include "scenario.h"

typedef struct {
    uint32_t timestamp_ms;
    float target_rpm;
    float measured_rpm;
    bool measured_rpm_valid;
    float controller_pwm_duty;
    float applied_pwm_duty;
    ControllerStatus controller_status;
    MachineStatus machine_status;
    FaultCode fault_code;
    const char *fault_message;
} TelemetryRecord;

typedef void (*TelemetrySink)(const TelemetryRecord *record, void *context);

bool simulation_run(
    ScenarioType scenario,
    uint32_t telemetry_interval_ms,
    TelemetrySink telemetry_sink,
    void *telemetry_context
);

#endif
