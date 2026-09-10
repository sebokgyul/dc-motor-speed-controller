#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>
#include <stdio.h>

#include "simulation.h"

typedef enum {
    TELEMETRY_TABLE = 0,
    TELEMETRY_JSON_LINES,
    TELEMETRY_CSV
} TelemetryFormat;

bool telemetry_format_parse(const char *text, TelemetryFormat *format);
const char *telemetry_format_name(TelemetryFormat format);
bool telemetry_write_header(FILE *stream, TelemetryFormat format);
bool telemetry_write_record(
    FILE *stream,
    TelemetryFormat format,
    const TelemetryRecord *record
);

#endif
