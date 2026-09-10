#include <stdbool.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "machine_monitor.h"
#include "motor_controller.h"
#include "motor_model.h"
#include "scenario.h"
#include "simulated_hal.h"
#include "simulation.h"
#include "telemetry.h"

#define SAMPLE_TIME_SECONDS CONTROL_SAMPLE_TIME_SECONDS

static int tests_run = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            printf("FAIL at line %d: %s\n", __LINE__, #condition); \
            return false; \
        } \
    } while (0)

static float absolute_value(float value)
{
    return value < 0.0f ? -value : value;
}

static bool float_is_close(float actual, float expected, float tolerance)
{
    return absolute_value(actual - expected) <= tolerance;
}

static bool controller_is_disabled_when_enable_is_low(void)
{
    MotorController controller;
    ControllerInput input = {2048, 1024, false};
    ControllerOutput output;

    motor_controller_init(&controller, 0.03f, 0.06f);
    output = motor_controller_update(&controller, input, SAMPLE_TIME_SECONDS);

    ASSERT_TRUE(output.status == CONTROLLER_DISABLED);
    ASSERT_TRUE(output.pwm_duty == 0.0f);
    ASSERT_TRUE(!output.fault_led);
    return true;
}

static bool adc_values_are_converted_to_rpm(void)
{
    MotorController controller;
    ControllerInput input = {2048, 1024, true};
    ControllerOutput output;

    motor_controller_init(&controller, 0.03f, 0.06f);
    output = motor_controller_update(&controller, input, SAMPLE_TIME_SECONDS);

    ASSERT_TRUE(float_is_close(output.target_rpm, 1500.0f, 1.0f));
    ASSERT_TRUE(float_is_close(output.measured_rpm, 750.0f, 1.0f));
    return true;
}

static bool positive_error_produces_pwm_output(void)
{
    MotorController controller;
    ControllerInput input = {2048, 0, true};
    ControllerOutput output;

    motor_controller_init(&controller, 0.03f, 0.06f);
    output = motor_controller_update(&controller, input, SAMPLE_TIME_SECONDS);

    ASSERT_TRUE(output.status == CONTROLLER_RUNNING);
    ASSERT_TRUE(output.pwm_duty > 0.0f);
    ASSERT_TRUE(output.pwm_duty <= 100.0f);
    return true;
}

static bool pwm_output_is_limited_to_valid_range(void)
{
    MotorController controller;
    ControllerInput high_input = {ADC_MAX_VALUE, 0, true};
    ControllerInput low_input = {0, ADC_MAX_VALUE, true};
    ControllerOutput output;

    motor_controller_init(&controller, 1.0f, 1.0f);
    output = motor_controller_update(&controller, high_input, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(output.pwm_duty == 100.0f);

    motor_controller_init(&controller, 1.0f, 1.0f);
    output = motor_controller_update(&controller, low_input, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(output.pwm_duty == 0.0f);
    return true;
}

static bool invalid_adc_value_activates_fault(void)
{
    MotorController controller;
    ControllerInput low_input = {-1, 0, true};
    ControllerInput high_input = {0, ADC_MAX_VALUE + 1, true};
    ControllerOutput output;

    motor_controller_init(&controller, 0.03f, 0.06f);
    output = motor_controller_update(&controller, low_input, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(output.status == CONTROLLER_INPUT_FAULT);
    ASSERT_TRUE(output.fault_reason == CONTROLLER_FAULT_TARGET_ADC);
    ASSERT_TRUE(output.fault_led);
    ASSERT_TRUE(output.pwm_duty == 0.0f);
    ASSERT_TRUE(float_is_close(output.target_rpm, 0.0f, 0.1f));

    output = motor_controller_update(&controller, high_input, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(output.status == CONTROLLER_INPUT_FAULT);
    ASSERT_TRUE(output.fault_reason == CONTROLLER_FAULT_SPEED_ADC);
    ASSERT_TRUE(output.fault_led);
    return true;
}

static bool valid_target_is_reported_during_sensor_fault(void)
{
    MotorController controller;
    ControllerInput input = {2048, -1, true};
    ControllerOutput output;

    motor_controller_init(&controller, 0.03f, 0.06f);
    output = motor_controller_update(&controller, input, SAMPLE_TIME_SECONDS);

    ASSERT_TRUE(output.status == CONTROLLER_INPUT_FAULT);
    ASSERT_TRUE(output.fault_reason == CONTROLLER_FAULT_SPEED_ADC);
    ASSERT_TRUE(float_is_close(output.target_rpm, 1500.0f, 1.0f));
    ASSERT_TRUE(!output.measured_rpm_valid);
    ASSERT_TRUE(output.measured_rpm == 0.0f);
    return true;
}

static bool invalid_sample_time_activates_fault(void)
{
    MotorController controller;
    ControllerInput input = {0, 0, true};
    ControllerOutput output;

    motor_controller_init(&controller, 0.03f, 0.06f);
    output = motor_controller_update(&controller, input, 0.0f);
    ASSERT_TRUE(output.status == CONTROLLER_INPUT_FAULT);
    ASSERT_TRUE(output.fault_reason == CONTROLLER_FAULT_SAMPLE_TIME);
    ASSERT_TRUE(output.fault_led);

    output = motor_controller_update(&controller, input, NAN);
    ASSERT_TRUE(output.status == CONTROLLER_INPUT_FAULT);
    ASSERT_TRUE(output.pwm_duty == 0.0f);

    output = motor_controller_update(&controller, input, INFINITY);
    ASSERT_TRUE(output.status == CONTROLLER_INPUT_FAULT);
    ASSERT_TRUE(output.pwm_duty == 0.0f);
    return true;
}

static bool invalid_controller_configuration_activates_fault(void)
{
    MotorController controller;
    ControllerInput input = {2048, 1024, true};
    ControllerOutput output;

    motor_controller_init(&controller, NAN, 0.06f);
    output = motor_controller_update(&controller, input, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(output.status == CONTROLLER_CONFIGURATION_FAULT);
    ASSERT_TRUE(output.fault_reason == CONTROLLER_FAULT_CONFIGURATION);
    ASSERT_TRUE(output.fault_led);
    ASSERT_TRUE(output.pwm_duty == 0.0f);

    motor_controller_init(&controller, 0.03f, -0.1f);
    output = motor_controller_update(&controller, input, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(output.status == CONTROLLER_CONFIGURATION_FAULT);
    return true;
}

static bool numeric_overflow_activates_fault(void)
{
    MotorController controller;
    ControllerInput input = {ADC_MAX_VALUE, 0, true};
    ControllerOutput output;

    motor_controller_init(&controller, 0.03f, 1.0f);
    output = motor_controller_update(&controller, input, FLT_MAX);

    ASSERT_TRUE(output.status == CONTROLLER_INPUT_FAULT);
    ASSERT_TRUE(output.fault_reason == CONTROLLER_FAULT_NUMERIC);
    ASSERT_TRUE(output.fault_led);
    ASSERT_TRUE(output.pwm_duty == 0.0f);
    ASSERT_TRUE(controller.integral == 0.0f);
    return true;
}

static bool disabled_controller_resets_integral(void)
{
    MotorController controller;
    ControllerInput running_input = {2048, 1024, true};
    ControllerInput disabled_input = {2048, 1024, false};

    motor_controller_init(&controller, 0.03f, 0.06f);
    motor_controller_update(&controller, running_input, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(controller.integral > 0.0f);

    motor_controller_update(&controller, disabled_input, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(controller.integral == 0.0f);
    return true;
}

static bool anti_windup_stops_integral_growth(void)
{
    MotorController controller;
    ControllerInput input = {ADC_MAX_VALUE, 0, true};

    motor_controller_init(&controller, 1.0f, 1.0f);
    motor_controller_update(&controller, input, SAMPLE_TIME_SECONDS);

    ASSERT_TRUE(controller.integral == 0.0f);
    return true;
}

static bool saturated_output_does_not_store_excess_integral(void)
{
    MotorController controller;
    ControllerInput high_input = {137, 0, true};
    ControllerInput low_input = {0, 137, true};
    ControllerOutput output;
    int update;

    motor_controller_init(&controller, 0.0f, 1.0f);
    controller.integral = 99.0f;
    for (update = 0; update < 5; update += 1) {
        output = motor_controller_update(&controller, high_input, 0.1f);
        ASSERT_TRUE(output.pwm_duty == 100.0f);
        ASSERT_TRUE(controller.integral == 99.0f);
    }

    motor_controller_init(&controller, 0.0f, 1.0f);
    controller.integral = 1.0f;
    for (update = 0; update < 5; update += 1) {
        output = motor_controller_update(&controller, low_input, 0.1f);
        ASSERT_TRUE(output.pwm_duty == 0.0f);
        ASSERT_TRUE(controller.integral == 1.0f);
    }
    return true;
}

static bool anti_windup_allows_integral_to_unwind(void)
{
    MotorController controller;
    ControllerInput input = {0, 137, true};
    ControllerOutput output;

    motor_controller_init(&controller, 0.0f, 1.0f);
    controller.integral = 100.0f;
    output = motor_controller_update(&controller, input, 0.1f);

    ASSERT_TRUE(output.pwm_duty < 100.0f);
    ASSERT_TRUE(controller.integral < 100.0f);
    return true;
}

static bool simulated_peripherals_store_io_values(void)
{
    SimulatedHal hal;

    hal_init(&hal);
    hal_set_inputs(&hal, 1200, 800, true);
    hal_pwm_write(&hal, 42.5f);
    hal_gpio_write_fault(&hal, true);

    ASSERT_TRUE(hal_adc_read(&hal, ADC_TARGET_SPEED) == 1200);
    ASSERT_TRUE(hal_adc_read(&hal, ADC_MEASURED_SPEED) == 800);
    ASSERT_TRUE(hal_gpio_read_enable(&hal));
    ASSERT_TRUE(hal_pwm_read(&hal) == 42.5f);
    ASSERT_TRUE(hal_gpio_read_fault(&hal));
    return true;
}

static bool simulated_pwm_rejects_unsafe_values(void)
{
    SimulatedHal hal;

    hal_init(&hal);
    hal_pwm_write(&hal, NAN);
    ASSERT_TRUE(hal_pwm_read(&hal) == 0.0f);

    hal_pwm_write(&hal, -10.0f);
    ASSERT_TRUE(hal_pwm_read(&hal) == 0.0f);

    hal_pwm_write(&hal, 120.0f);
    ASSERT_TRUE(hal_pwm_read(&hal) == 100.0f);
    ASSERT_TRUE(hal_adc_read(&hal, (AdcChannel)99) == -1);
    return true;
}

static bool closed_loop_reaches_target_speed(void)
{
    MotorController controller;
    MotorModel motor;
    ControllerInput input;
    ControllerOutput output = {0};
    int step;

    motor_controller_init(&controller, 0.03f, 0.06f);
    motor_model_init(&motor, 0.35f);

    input.target_adc = motor_model_rpm_to_adc(1800.0f);
    input.enable = true;

    for (step = 0; step < 600; step += 1) {
        input.speed_adc = motor_model_speed_adc(&motor);
        output = motor_controller_update(&controller, input, SAMPLE_TIME_SECONDS);
        motor_model_update(&motor, output.pwm_duty, SAMPLE_TIME_SECONDS);

        if (step >= 500) {
            ASSERT_TRUE(float_is_close(motor.speed_rpm, 1800.0f, 20.0f));
        }
    }

    ASSERT_TRUE(output.status == CONTROLLER_RUNNING);
    return true;
}

static bool invalid_motor_model_step_keeps_safe_speed(void)
{
    MotorModel motor;

    motor_model_init(&motor, 0.0f);
    motor_model_update(&motor, 100.0f, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(motor.speed_rpm == 0.0f);

    motor_model_init(&motor, 0.35f);
    motor_model_update(&motor, 100.0f, NAN);
    ASSERT_TRUE(motor.speed_rpm == 0.0f);

    motor_model_update(&motor, NAN, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(motor.speed_rpm == 0.0f);

    motor.speed_rpm = 100.0f;
    motor_model_update(&motor, 0.0f, FLT_MAX);
    ASSERT_TRUE(motor.speed_rpm == 100.0f);
    return true;
}

static bool motor_model_handles_load_and_non_finite_conversion(void)
{
    MotorModel motor;

    ASSERT_TRUE(motor_model_rpm_to_adc(NAN) == -1);
    ASSERT_TRUE(motor_model_rpm_to_adc(INFINITY) == -1);
    ASSERT_TRUE(motor_model_rpm_to_adc(-100.0f) == 0);
    ASSERT_TRUE(motor_model_rpm_to_adc(4000.0f) == ADC_MAX_VALUE);

    motor_model_init(&motor, 0.35f);
    motor.speed_rpm = NAN;
    ASSERT_TRUE(motor_model_speed_adc(&motor) == -1);

    motor_model_init(&motor, 0.35f);
    motor_model_set_load(&motor, 1.0f);
    motor_model_update(&motor, 100.0f, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(motor.speed_rpm == 0.0f);

    motor_model_set_load(&motor, NAN);
    motor_model_update(&motor, 100.0f, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(motor.speed_rpm == 0.0f);
    return true;
}

static bool scenarios_are_generated_at_defined_boundaries(void)
{
    ScenarioType parsed;
    ScenarioFrame frame;

    ASSERT_TRUE(scenario_parse("normal", &parsed));
    ASSERT_TRUE(parsed == SCENARIO_NORMAL);
    ASSERT_TRUE(scenario_parse("sensor-disconnect", &parsed));
    ASSERT_TRUE(parsed == SCENARIO_SENSOR_DISCONNECT);
    ASSERT_TRUE(!scenario_parse("unsupported", &parsed));

    frame = scenario_frame_at(SCENARIO_NORMAL, 999U);
    ASSERT_TRUE(!frame.operator_enable);
    frame = scenario_frame_at(SCENARIO_NORMAL, 1000U);
    ASSERT_TRUE(frame.operator_enable);
    ASSERT_TRUE(frame.target_rpm == 1800.0f);
    frame = scenario_frame_at(SCENARIO_NORMAL, 5000U);
    ASSERT_TRUE(frame.target_rpm == 2500.0f);
    frame = scenario_frame_at(SCENARIO_NORMAL, 10000U);
    ASSERT_TRUE(!frame.operator_enable);

    frame = scenario_frame_at(SCENARIO_SENSOR_DISCONNECT, 5500U);
    ASSERT_TRUE(!frame.speed_sensor_connected);
    frame = scenario_frame_at(SCENARIO_MECHANICAL_JAM, 5500U);
    ASSERT_TRUE(frame.motor_load_fraction == 1.0f);
    return true;
}

static ControllerOutput running_output(
    float target_rpm,
    float measured_rpm,
    float pwm_duty
)
{
    ControllerOutput output = {0};

    output.target_rpm = target_rpm;
    output.measured_rpm = measured_rpm;
    output.measured_rpm_valid = true;
    output.pwm_duty = pwm_duty;
    output.status = CONTROLLER_RUNNING;
    return output;
}

static bool machine_monitor_warns_then_latches_stall(void)
{
    MachineMonitor monitor;
    ControllerOutput output = running_output(2000.0f, 200.0f, 80.0f);
    int sample;

    machine_monitor_init(&monitor);
    for (sample = 0; sample < 75; sample += 1) {
        ASSERT_TRUE(machine_monitor_update(
            &monitor,
            output,
            true,
            SAMPLE_TIME_SECONDS
        ) == MACHINE_WARNING);
    }

    ASSERT_TRUE(machine_monitor_update(
        &monitor,
        output,
        true,
        SAMPLE_TIME_SECONDS
    ) == MACHINE_FAULT);
    ASSERT_TRUE(monitor.fault_code == FAULT_MOTOR_STALL);
    ASSERT_TRUE(float_is_close(monitor.stall_elapsed_seconds, 0.75f, 0.00001f));
    ASSERT_TRUE(machine_monitor_stop_requested(&monitor));

    output = running_output(2000.0f, 2000.0f, 50.0f);
    ASSERT_TRUE(machine_monitor_update(
        &monitor,
        output,
        true,
        SAMPLE_TIME_SECONDS
    ) == MACHINE_FAULT);

    machine_monitor_init(&monitor);
    ASSERT_TRUE(monitor.status == MACHINE_DISABLED);
    ASSERT_TRUE(monitor.fault_code == FAULT_NONE);
    ASSERT_TRUE(!monitor.stall_active);
    ASSERT_TRUE(!machine_monitor_stop_requested(&monitor));
    return true;
}

static bool machine_monitor_clears_unconfirmed_stall_timer(void)
{
    MachineMonitor monitor;
    ControllerOutput stalled = running_output(2000.0f, 200.0f, 80.0f);
    ControllerOutput recovered = running_output(2000.0f, 1500.0f, 70.0f);

    machine_monitor_init(&monitor);
    machine_monitor_update(&monitor, stalled, true, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(monitor.stall_active);
    ASSERT_TRUE(monitor.stall_elapsed_seconds == 0.0f);
    ASSERT_TRUE(machine_monitor_update(
        &monitor,
        recovered,
        true,
        SAMPLE_TIME_SECONDS
    ) == MACHINE_RUNNING);
    ASSERT_TRUE(monitor.stall_elapsed_seconds == 0.0f);
    ASSERT_TRUE(!monitor.stall_active);
    return true;
}

static bool machine_monitor_latches_sensor_fault(void)
{
    MachineMonitor monitor;
    ControllerOutput output = {0};

    machine_monitor_init(&monitor);
    output.status = CONTROLLER_INPUT_FAULT;
    output.fault_reason = CONTROLLER_FAULT_SPEED_ADC;

    ASSERT_TRUE(machine_monitor_update(
        &monitor,
        output,
        true,
        SAMPLE_TIME_SECONDS
    ) == MACHINE_FAULT);
    ASSERT_TRUE(monitor.fault_code == FAULT_SENSOR_SIGNAL_INVALID);
    ASSERT_TRUE(machine_monitor_stop_requested(&monitor));
    return true;
}

static bool machine_monitor_rejects_other_invalid_controller_state(void)
{
    MachineMonitor monitor;
    ControllerOutput output = {0};

    machine_monitor_init(&monitor);
    output.status = CONTROLLER_INPUT_FAULT;
    output.fault_reason = CONTROLLER_FAULT_TARGET_ADC;
    ASSERT_TRUE(machine_monitor_update(
        &monitor,
        output,
        true,
        SAMPLE_TIME_SECONDS
    ) == MACHINE_FAULT);
    ASSERT_TRUE(monitor.fault_code == FAULT_CONTROLLER_INPUT_INVALID);

    machine_monitor_init(&monitor);
    output.status = (ControllerStatus)99;
    output.fault_reason = CONTROLLER_FAULT_NONE;
    ASSERT_TRUE(machine_monitor_update(
        &monitor,
        output,
        true,
        SAMPLE_TIME_SECONDS
    ) == MACHINE_FAULT);
    ASSERT_TRUE(monitor.fault_code == FAULT_CONTROLLER_CONFIGURATION);

    machine_monitor_init(&monitor);
    output = running_output(2000.0f, 200.0f, 80.0f);
    output.measured_rpm_valid = false;
    ASSERT_TRUE(machine_monitor_update(
        &monitor,
        output,
        true,
        SAMPLE_TIME_SECONDS
    ) == MACHINE_FAULT);
    ASSERT_TRUE(monitor.fault_code == FAULT_SENSOR_SIGNAL_INVALID);
    return true;
}

#define EXPECTED_TELEMETRY_RECORDS 111

typedef struct {
    TelemetryRecord records[EXPECTED_TELEMETRY_RECORDS];
    int count;
} SimulationCapture;

typedef struct {
    bool warning_seen;
    bool fault_seen;
    bool nonzero_fault_pwm_seen;
    uint32_t first_warning_timestamp_ms;
    uint32_t first_fault_timestamp_ms;
} FaultSafetyCapture;

static void capture_record(const TelemetryRecord *record, void *context)
{
    SimulationCapture *capture = context;

    if (capture->count < EXPECTED_TELEMETRY_RECORDS) {
        capture->records[capture->count] = *record;
    }
    capture->count += 1;
}

static SimulationCapture run_capture(ScenarioType scenario)
{
    SimulationCapture capture = {0};

    simulation_run(scenario, 100U, capture_record, &capture);
    return capture;
}

static void capture_fault_safety(const TelemetryRecord *record, void *context)
{
    FaultSafetyCapture *capture = context;

    if (record->machine_status == MACHINE_WARNING && !capture->warning_seen) {
        capture->warning_seen = true;
        capture->first_warning_timestamp_ms = record->timestamp_ms;
    }

    if (record->machine_status != MACHINE_FAULT) {
        return;
    }

    if (!capture->fault_seen) {
        capture->fault_seen = true;
        capture->first_fault_timestamp_ms = record->timestamp_ms;
    }
    if (record->applied_pwm_duty != 0.0f) {
        capture->nonzero_fault_pwm_seen = true;
    }
}

static bool records_are_equal(
    const TelemetryRecord *first,
    const TelemetryRecord *second
)
{
    return first->timestamp_ms == second->timestamp_ms
        && first->target_rpm == second->target_rpm
        && first->measured_rpm == second->measured_rpm
        && first->measured_rpm_valid == second->measured_rpm_valid
        && first->controller_pwm_duty == second->controller_pwm_duty
        && first->applied_pwm_duty == second->applied_pwm_duty
        && first->controller_status == second->controller_status
        && first->machine_status == second->machine_status
        && first->fault_code == second->fault_code
        && strcmp(first->fault_message, second->fault_message) == 0;
}

static bool complete_simulations_are_deterministic(void)
{
    SimulationCapture first_normal = run_capture(SCENARIO_NORMAL);
    SimulationCapture second_normal = run_capture(SCENARIO_NORMAL);
    SimulationCapture sensor = run_capture(SCENARIO_SENSOR_DISCONNECT);
    SimulationCapture jam = run_capture(SCENARIO_MECHANICAL_JAM);
    FaultSafetyCapture jam_safety = {0};
    int record;

    ASSERT_TRUE(first_normal.count == EXPECTED_TELEMETRY_RECORDS);
    ASSERT_TRUE(second_normal.count == EXPECTED_TELEMETRY_RECORDS);
    ASSERT_TRUE(sensor.count == EXPECTED_TELEMETRY_RECORDS);
    ASSERT_TRUE(jam.count == EXPECTED_TELEMETRY_RECORDS);

    for (record = 0; record < EXPECTED_TELEMETRY_RECORDS; record += 1) {
        ASSERT_TRUE(records_are_equal(
            &first_normal.records[record],
            &second_normal.records[record]
        ));
    }

    ASSERT_TRUE(first_normal.records[110].fault_code == FAULT_NONE);
    ASSERT_TRUE(first_normal.records[110].machine_status == MACHINE_DISABLED);

    ASSERT_TRUE(sensor.records[54].fault_code == FAULT_NONE);
    ASSERT_TRUE(sensor.records[55].timestamp_ms == 5500U);
    ASSERT_TRUE(sensor.records[55].fault_code == FAULT_SENSOR_SIGNAL_INVALID);
    ASSERT_TRUE(sensor.records[55].controller_status == CONTROLLER_INPUT_FAULT);
    ASSERT_TRUE(!sensor.records[55].measured_rpm_valid);
    ASSERT_TRUE(sensor.records[55].controller_pwm_duty == 0.0f);
    ASSERT_TRUE(sensor.records[55].applied_pwm_duty == 0.0f);

    ASSERT_TRUE(jam.records[59].machine_status == MACHINE_RUNNING);
    ASSERT_TRUE(jam.records[60].timestamp_ms == 6000U);
    ASSERT_TRUE(jam.records[60].machine_status == MACHINE_WARNING);
    ASSERT_TRUE(jam.records[66].machine_status == MACHINE_WARNING);
    ASSERT_TRUE(jam.records[67].timestamp_ms == 6700U);
    ASSERT_TRUE(jam.records[67].fault_code == FAULT_MOTOR_STALL);
    ASSERT_TRUE(jam.records[67].controller_status == CONTROLLER_RUNNING);
    ASSERT_TRUE(jam.records[67].controller_pwm_duty == 100.0f);
    ASSERT_TRUE(jam.records[67].applied_pwm_duty == 0.0f);
    ASSERT_TRUE(jam.records[68].controller_status == CONTROLLER_DISABLED);
    ASSERT_TRUE(jam.records[68].controller_pwm_duty == 0.0f);

    ASSERT_TRUE(simulation_run(
        SCENARIO_MECHANICAL_JAM,
        CONTROL_SAMPLE_TIME_MS,
        capture_fault_safety,
        &jam_safety
    ));
    ASSERT_TRUE(jam_safety.warning_seen);
    ASSERT_TRUE(jam_safety.fault_seen);
    ASSERT_TRUE(jam_safety.first_fault_timestamp_ms == 6700U);
    ASSERT_TRUE(
        jam_safety.first_fault_timestamp_ms
            - jam_safety.first_warning_timestamp_ms
        == 750U
    );
    ASSERT_TRUE(!jam_safety.nonzero_fault_pwm_seen);
    return true;
}

static TelemetryRecord example_telemetry_record(void)
{
    TelemetryRecord record;

    record.timestamp_ms = 1200U;
    record.target_rpm = 1800.0f;
    record.measured_rpm = 1500.0f;
    record.measured_rpm_valid = true;
    record.controller_pwm_duty = 55.0f;
    record.applied_pwm_duty = 55.0f;
    record.controller_status = CONTROLLER_RUNNING;
    record.machine_status = MACHINE_WARNING;
    record.fault_code = FAULT_NONE;
    record.fault_message = fault_message(FAULT_NONE);
    return record;
}

static bool telemetry_json_contains_required_fields(void)
{
    FILE *stream = tmpfile();
    TelemetryRecord record = example_telemetry_record();
    char line[1024];

    ASSERT_TRUE(stream != NULL);
    ASSERT_TRUE(telemetry_write_record(stream, TELEMETRY_JSON_LINES, &record));
    rewind(stream);
    ASSERT_TRUE(fgets(line, sizeof(line), stream) != NULL);
    ASSERT_TRUE(strstr(line, "\"timestamp_ms\":1200") != NULL);
    ASSERT_TRUE(strstr(line, "\"target_rpm\":1800.0") != NULL);
    ASSERT_TRUE(strstr(line, "\"measured_rpm\":1500.0") != NULL);
    ASSERT_TRUE(strstr(line, "\"controller_pwm_duty\":55.0") != NULL);
    ASSERT_TRUE(strstr(line, "\"applied_pwm_duty\":55.0") != NULL);
    ASSERT_TRUE(strstr(line, "\"controller_status\":\"running\"") != NULL);
    ASSERT_TRUE(strstr(line, "\"machine_status\":\"warning\"") != NULL);
    ASSERT_TRUE(strstr(line, "\"fault_code\":\"NONE\"") != NULL);
    ASSERT_TRUE(strstr(line, "\"fault_message\":\"No active fault.\"") != NULL);
    ASSERT_TRUE(fclose(stream) == 0);

    stream = tmpfile();
    ASSERT_TRUE(stream != NULL);
    record.measured_rpm_valid = false;
    ASSERT_TRUE(telemetry_write_record(stream, TELEMETRY_JSON_LINES, &record));
    rewind(stream);
    ASSERT_TRUE(fgets(line, sizeof(line), stream) != NULL);
    ASSERT_TRUE(strstr(line, "\"measured_rpm\":null") != NULL);
    ASSERT_TRUE(fclose(stream) == 0);

    stream = tmpfile();
    ASSERT_TRUE(stream != NULL);
    ASSERT_TRUE(telemetry_write_record(stream, TELEMETRY_CSV, &record));
    rewind(stream);
    ASSERT_TRUE(fgets(line, sizeof(line), stream) != NULL);
    ASSERT_TRUE(strstr(line, "1200,1800.0,,55.0,55.0,") != NULL);
    ASSERT_TRUE(fclose(stream) == 0);

    stream = tmpfile();
    ASSERT_TRUE(stream != NULL);
    ASSERT_TRUE(telemetry_write_record(stream, TELEMETRY_TABLE, &record));
    rewind(stream);
    ASSERT_TRUE(fgets(line, sizeof(line), stream) != NULL);
    ASSERT_TRUE(strstr(line, "|       -- |") != NULL);
    ASSERT_TRUE(fclose(stream) == 0);
    return true;
}

static bool telemetry_escapes_strings_and_rejects_invalid_numbers(void)
{
    FILE *stream = tmpfile();
    TelemetryRecord record = example_telemetry_record();
    char line[1024];

    ASSERT_TRUE(stream != NULL);
    record.fault_message = "Quoted \"value\" and slash \\ with line\nend";
    ASSERT_TRUE(telemetry_write_record(stream, TELEMETRY_JSON_LINES, &record));
    rewind(stream);
    ASSERT_TRUE(fgets(line, sizeof(line), stream) != NULL);
    ASSERT_TRUE(strstr(line, "Quoted \\\"value\\\"") != NULL);
    ASSERT_TRUE(strstr(line, "slash \\\\ with line\\nend") != NULL);
    ASSERT_TRUE(fclose(stream) == 0);

    stream = tmpfile();
    ASSERT_TRUE(stream != NULL);
    record.fault_message = "A \"quoted\" note";
    ASSERT_TRUE(telemetry_write_record(stream, TELEMETRY_CSV, &record));
    rewind(stream);
    ASSERT_TRUE(fgets(line, sizeof(line), stream) != NULL);
    ASSERT_TRUE(strstr(line, "\"A \"\"quoted\"\" note\"") != NULL);
    ASSERT_TRUE(fclose(stream) == 0);

    record.fault_message = NULL;
    ASSERT_TRUE(!telemetry_write_record(stdout, TELEMETRY_JSON_LINES, &record));
    record.fault_message = fault_message(FAULT_NONE);
    record.measured_rpm = NAN;
    ASSERT_TRUE(!telemetry_write_record(stdout, TELEMETRY_JSON_LINES, &record));
    ASSERT_TRUE(!telemetry_write_header(stdout, (TelemetryFormat)99));
    return true;
}

static void run_test(const char *name, bool (*test_function)(void))
{
    tests_run += 1;

    if (test_function()) {
        printf("PASS: %s\n", name);
    } else {
        tests_failed += 1;
    }
}

int main(void)
{
    run_test("disabled state", controller_is_disabled_when_enable_is_low);
    run_test("ADC conversion", adc_values_are_converted_to_rpm);
    run_test("positive speed error", positive_error_produces_pwm_output);
    run_test("PWM limits", pwm_output_is_limited_to_valid_range);
    run_test("invalid ADC", invalid_adc_value_activates_fault);
    run_test("target retained during sensor fault", valid_target_is_reported_during_sensor_fault);
    run_test("invalid sample time", invalid_sample_time_activates_fault);
    run_test("invalid controller configuration", invalid_controller_configuration_activates_fault);
    run_test("controller numeric overflow", numeric_overflow_activates_fault);
    run_test("integral reset", disabled_controller_resets_integral);
    run_test("anti windup", anti_windup_stops_integral_growth);
    run_test("anti windup saturation", saturated_output_does_not_store_excess_integral);
    run_test("anti windup recovery", anti_windup_allows_integral_to_unwind);
    run_test("simulated peripherals", simulated_peripherals_store_io_values);
    run_test("safe simulated PWM", simulated_pwm_rejects_unsafe_values);
    run_test("closed loop response", closed_loop_reaches_target_speed);
    run_test("invalid motor model step", invalid_motor_model_step_keeps_safe_speed);
    run_test("motor load and conversion safety", motor_model_handles_load_and_non_finite_conversion);
    run_test("scenario boundaries", scenarios_are_generated_at_defined_boundaries);
    run_test("stall monitor", machine_monitor_warns_then_latches_stall);
    run_test("stall timer recovery", machine_monitor_clears_unconfirmed_stall_timer);
    run_test("sensor fault monitor", machine_monitor_latches_sensor_fault);
    run_test("other controller faults", machine_monitor_rejects_other_invalid_controller_state);
    run_test("deterministic full simulations", complete_simulations_are_deterministic);
    run_test("telemetry fields and nulls", telemetry_json_contains_required_fields);
    run_test("telemetry escaping and validation", telemetry_escapes_strings_and_rejects_invalid_numbers);

    printf("\n%d tests, %d failures\n", tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
