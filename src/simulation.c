#include "simulation.h"

#include <stddef.h>

#include "motor_config.h"
#include "motor_model.h"
#include "simulated_hal.h"

#define DISCONNECTED_ADC_VALUE (-1)

bool simulation_run(
    ScenarioType scenario,
    uint32_t telemetry_interval_ms,
    TelemetrySink telemetry_sink,
    void *telemetry_context
)
{
    MotorController controller;
    MotorModel motor;
    SimulatedHal hal;
    MachineMonitor monitor;
    uint32_t timestamp_ms;

    if (scenario < SCENARIO_NORMAL || scenario >= SCENARIO_COUNT
        || telemetry_interval_ms == 0U
        || telemetry_interval_ms % CONTROL_SAMPLE_TIME_MS != 0U) {
        return false;
    }

    motor_controller_init(&controller, 0.03f, 0.06f);
    motor_model_init(&motor, 0.35f);
    hal_init(&hal);
    machine_monitor_init(&monitor);

    for (timestamp_ms = 0U;
         timestamp_ms <= SIMULATION_DURATION_MS;
         timestamp_ms += CONTROL_SAMPLE_TIME_MS) {
        ScenarioFrame frame = scenario_frame_at(scenario, timestamp_ms);
        ControllerInput input;
        ControllerOutput output;
        MachineStatus machine_status;
        bool local_enable = frame.operator_enable
            && !machine_monitor_stop_requested(&monitor);
        int speed_adc;

        motor_model_set_load(&motor, frame.motor_load_fraction);
        speed_adc = frame.speed_sensor_connected
            ? motor_model_speed_adc(&motor)
            : DISCONNECTED_ADC_VALUE;
        hal_set_inputs(
            &hal,
            motor_model_rpm_to_adc(frame.target_rpm),
            speed_adc,
            local_enable
        );

        input.target_adc = hal_adc_read(&hal, ADC_TARGET_SPEED);
        input.speed_adc = hal_adc_read(&hal, ADC_MEASURED_SPEED);
        input.enable = hal_gpio_read_enable(&hal);

        output = motor_controller_update(
            &controller,
            input,
            CONTROL_SAMPLE_TIME_SECONDS
        );
        machine_status = machine_monitor_update(
            &monitor,
            output,
            frame.operator_enable,
            CONTROL_SAMPLE_TIME_SECONDS
        );
        hal_pwm_write(
            &hal,
            machine_monitor_stop_requested(&monitor) ? 0.0f : output.pwm_duty
        );
        motor_model_update(
            &motor,
            hal_pwm_read(&hal),
            CONTROL_SAMPLE_TIME_SECONDS
        );
        hal_gpio_write_fault(
            &hal,
            output.fault_led || machine_status == MACHINE_FAULT
        );

        if (telemetry_sink != NULL
            && timestamp_ms % telemetry_interval_ms == 0U) {
            TelemetryRecord record;

            record.timestamp_ms = timestamp_ms;
            record.target_rpm = output.target_rpm;
            record.measured_rpm = output.measured_rpm;
            record.measured_rpm_valid = output.measured_rpm_valid;
            record.pwm_duty = hal_pwm_read(&hal);
            record.controller_status = output.status;
            record.machine_status = machine_status;
            record.fault_code = monitor.fault_code;
            record.fault_message = fault_message(monitor.fault_code);
            telemetry_sink(&record, telemetry_context);
        }
    }

    return true;
}
