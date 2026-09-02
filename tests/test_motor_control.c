#include <stdbool.h>
#include <stdio.h>
#include <math.h>

#include "motor_controller.h"
#include "motor_model.h"
#include "simulated_hal.h"

#define SAMPLE_TIME_SECONDS 0.01f

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
    ASSERT_TRUE(output.fault_led);
    ASSERT_TRUE(output.pwm_duty == 0.0f);

    output = motor_controller_update(&controller, high_input, SAMPLE_TIME_SECONDS);
    ASSERT_TRUE(output.status == CONTROLLER_INPUT_FAULT);
    ASSERT_TRUE(output.fault_led);
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
    ASSERT_TRUE(output.fault_led);

    output = motor_controller_update(&controller, input, NAN);
    ASSERT_TRUE(output.status == CONTROLLER_INPUT_FAULT);
    ASSERT_TRUE(output.pwm_duty == 0.0f);

    output = motor_controller_update(&controller, input, INFINITY);
    ASSERT_TRUE(output.status == CONTROLLER_INPUT_FAULT);
    ASSERT_TRUE(output.pwm_duty == 0.0f);
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

static bool saturated_output_reaches_pwm_limit(void)
{
    MotorController controller;
    ControllerInput input = {137, 0, true};
    ControllerOutput output;

    motor_controller_init(&controller, 0.0f, 1.0f);
    controller.integral = 99.0f;
    output = motor_controller_update(&controller, input, 0.1f);

    ASSERT_TRUE(output.pwm_duty == 100.0f);
    ASSERT_TRUE(controller.integral == 99.0f);
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
    run_test("invalid sample time", invalid_sample_time_activates_fault);
    run_test("integral reset", disabled_controller_resets_integral);
    run_test("anti windup", anti_windup_stops_integral_growth);
    run_test("PWM saturation boundary", saturated_output_reaches_pwm_limit);
    run_test("simulated peripherals", simulated_peripherals_store_io_values);
    run_test("closed loop response", closed_loop_reaches_target_speed);
    run_test("invalid motor model step", invalid_motor_model_step_keeps_safe_speed);

    printf("\n%d tests, %d failures\n", tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
