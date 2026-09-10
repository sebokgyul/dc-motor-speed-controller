#include "scenario.h"

#include <string.h>

#define SENSOR_DISCONNECT_TIME_MS 5500U
#define MECHANICAL_JAM_TIME_MS 5500U

static ScenarioFrame normal_frame_at(uint32_t timestamp_ms)
{
    ScenarioFrame frame = {0.0f, false, true, 0.0f};

    if (timestamp_ms < 1000U || timestamp_ms >= 10000U) {
        return frame;
    }

    frame.operator_enable = true;
    if (timestamp_ms < 5000U) {
        frame.target_rpm = 1800.0f;
    } else if (timestamp_ms < 8000U) {
        frame.target_rpm = 2500.0f;
    } else {
        frame.target_rpm = 1200.0f;
    }

    return frame;
}

bool scenario_parse(const char *text, ScenarioType *scenario)
{
    ScenarioType candidate;

    for (candidate = SCENARIO_NORMAL; candidate < SCENARIO_COUNT; candidate += 1) {
        if (strcmp(text, scenario_name(candidate)) == 0) {
            *scenario = candidate;
            return true;
        }
    }

    return false;
}

const char *scenario_name(ScenarioType scenario)
{
    switch (scenario) {
        case SCENARIO_NORMAL:
            return "normal";
        case SCENARIO_SENSOR_DISCONNECT:
            return "sensor-disconnect";
        case SCENARIO_MECHANICAL_JAM:
            return "mechanical-jam";
        default:
            return "unknown";
    }
}

ScenarioFrame scenario_frame_at(ScenarioType scenario, uint32_t timestamp_ms)
{
    ScenarioFrame frame = normal_frame_at(timestamp_ms);

    if (scenario == SCENARIO_SENSOR_DISCONNECT
        && timestamp_ms >= SENSOR_DISCONNECT_TIME_MS) {
        frame.speed_sensor_connected = false;
    }

    if (scenario == SCENARIO_MECHANICAL_JAM
        && timestamp_ms >= MECHANICAL_JAM_TIME_MS) {
        frame.motor_load_fraction = 1.0f;
    }

    return frame;
}
