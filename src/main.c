#include <stdbool.h>
#include <stdio.h>

#include "motor_controller.h"
#include "motor_model.h"
#include "simulated_hal.h"

#define SAMPLE_TIME_SECONDS 0.01f
#define SIMULATION_TIME_SECONDS 11.0f
#define PRINT_INTERVAL_STEPS 50

static float target_speed_at(float time_seconds)
{
    if (time_seconds < 1.0f) {
        return 0.0f;
    }

    if (time_seconds < 5.0f) {
        return 1800.0f;
    }

    if (time_seconds < 8.0f) {
        return 2500.0f;
    }

    if (time_seconds < 10.0f) {
        return 1200.0f;
    }

    return 0.0f;
}

static bool motor_is_enabled(float time_seconds)
{
    return time_seconds >= 1.0f && time_seconds < 10.0f;
}

int main(void)
{
    MotorController controller;
    MotorModel motor;
    SimulatedHal hal;
    int step;
    int total_steps = (int)(SIMULATION_TIME_SECONDS / SAMPLE_TIME_SECONDS);

    motor_controller_init(&controller, 0.03f, 0.06f);
    motor_model_init(&motor, 0.35f);
    hal_init(&hal);

    printf(" time | target | measured | pwm duty | status\n");
    printf("======+========+==========+==========+=============\n");

    for (step = 0; step <= total_steps; step += 1) {
        float time_seconds = (float)step * SAMPLE_TIME_SECONDS;
        ControllerInput input;
        ControllerOutput output;

        hal_set_inputs(
            &hal,
            motor_model_rpm_to_adc(target_speed_at(time_seconds)),
            motor_model_speed_adc(&motor),
            motor_is_enabled(time_seconds)
        );

        input.target_adc = hal_adc_read(&hal, ADC_TARGET_SPEED);
        input.speed_adc = hal_adc_read(&hal, ADC_MEASURED_SPEED);
        input.enable = hal_gpio_read_enable(&hal);

        output = motor_controller_update(&controller, input, SAMPLE_TIME_SECONDS);
        hal_pwm_write(&hal, output.pwm_duty);
        hal_gpio_write_fault(&hal, output.fault_led);
        motor_model_update(&motor, hal_pwm_read(&hal), SAMPLE_TIME_SECONDS);

        if (step % PRINT_INTERVAL_STEPS == 0) {
            printf(
                "%5.1f | %6.0f | %8.0f | %8.1f | %s\n",
                (double)time_seconds,
                (double)output.target_rpm,
                (double)output.measured_rpm,
                (double)output.pwm_duty,
                controller_status_name(output.status)
            );
        }
    }

    return hal_gpio_read_fault(&hal) ? 1 : 0;
}
