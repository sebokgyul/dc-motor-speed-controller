#ifndef SCENARIO_H
#define SCENARIO_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SCENARIO_NORMAL = 0,
    SCENARIO_SENSOR_DISCONNECT,
    SCENARIO_MECHANICAL_JAM,
    SCENARIO_COUNT
} ScenarioType;

typedef struct {
    float target_rpm;
    bool operator_enable;
    bool speed_sensor_connected;
    float motor_load_fraction;
} ScenarioFrame;

bool scenario_parse(const char *text, ScenarioType *scenario);
const char *scenario_name(ScenarioType scenario);
ScenarioFrame scenario_frame_at(ScenarioType scenario, uint32_t timestamp_ms);

#endif
