#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "scenario.h"
#include "simulation.h"
#include "telemetry.h"

#define TABLE_INTERVAL_MS 500U
#define MACHINE_READABLE_INTERVAL_MS 100U

typedef struct {
    TelemetryFormat format;
    bool write_failed;
} OutputContext;

static bool print_usage(FILE *stream, const char *program_name)
{
    return fprintf(
        stream,
        "Usage: %s [--scenario normal|sensor-disconnect|mechanical-jam]\n"
        "          [--format table|jsonl|csv]\n",
        program_name
    ) >= 0;
}

static void write_telemetry(const TelemetryRecord *record, void *context)
{
    OutputContext *output = context;

    if (!telemetry_write_record(stdout, output->format, record)) {
        output->write_failed = true;
    }
}

int main(int argc, char **argv)
{
    ScenarioType scenario = SCENARIO_NORMAL;
    TelemetryFormat format = TELEMETRY_TABLE;
    OutputContext output;
    uint32_t interval_ms;
    int argument;

    for (argument = 1; argument < argc; argument += 1) {
        if (strcmp(argv[argument], "--help") == 0) {
            if (!print_usage(stdout, argv[0])
                || fflush(stdout) != 0
                || ferror(stdout)) {
                fprintf(stderr, "Help output failed.\n");
                return 1;
            }
            return 0;
        }

        if (strcmp(argv[argument], "--scenario") == 0) {
            argument += 1;
            if (argument >= argc || !scenario_parse(argv[argument], &scenario)) {
                fprintf(stderr, "Invalid or missing scenario.\n");
                print_usage(stderr, argv[0]);
                return 2;
            }
        } else if (strcmp(argv[argument], "--format") == 0) {
            argument += 1;
            if (argument >= argc || !telemetry_format_parse(argv[argument], &format)) {
                fprintf(stderr, "Invalid or missing telemetry format.\n");
                print_usage(stderr, argv[0]);
                return 2;
            }
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[argument]);
            print_usage(stderr, argv[0]);
            return 2;
        }
    }

    output.format = format;
    output.write_failed = false;
    interval_ms = format == TELEMETRY_TABLE
        ? TABLE_INTERVAL_MS
        : MACHINE_READABLE_INTERVAL_MS;

    if (!telemetry_write_header(stdout, format)
        || !simulation_run(scenario, interval_ms, write_telemetry, &output)
        || output.write_failed
        || fflush(stdout) != 0
        || ferror(stdout)) {
        fprintf(stderr, "Simulation output failed.\n");
        return 1;
    }

    return 0;
}
