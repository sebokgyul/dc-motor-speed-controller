#include "telemetry.h"

#include <math.h>
#include <string.h>

static bool write_json_string(FILE *stream, const char *text)
{
    const unsigned char *character;

    if (text == NULL || fputc('"', stream) == EOF) {
        return false;
    }

    for (character = (const unsigned char *)text; *character != '\0'; character += 1) {
        int result;

        switch (*character) {
            case '"':
                result = fputs("\\\"", stream);
                break;
            case '\\':
                result = fputs("\\\\", stream);
                break;
            case '\b':
                result = fputs("\\b", stream);
                break;
            case '\f':
                result = fputs("\\f", stream);
                break;
            case '\n':
                result = fputs("\\n", stream);
                break;
            case '\r':
                result = fputs("\\r", stream);
                break;
            case '\t':
                result = fputs("\\t", stream);
                break;
            default:
                if (*character < 0x20U) {
                    result = fprintf(stream, "\\u%04x", (unsigned int)*character);
                } else {
                    result = fputc((int)*character, stream);
                }
                break;
        }

        if (result < 0) {
            return false;
        }
    }

    return fputc('"', stream) != EOF;
}

static bool write_csv_string(FILE *stream, const char *text)
{
    const char *character;

    if (text == NULL || fputc('"', stream) == EOF) {
        return false;
    }

    for (character = text; *character != '\0'; character += 1) {
        if (*character == '"' && fputc('"', stream) == EOF) {
            return false;
        }
        if (fputc(*character, stream) == EOF) {
            return false;
        }
    }

    return fputc('"', stream) != EOF;
}

bool telemetry_format_parse(const char *text, TelemetryFormat *format)
{
    if (strcmp(text, "table") == 0) {
        *format = TELEMETRY_TABLE;
        return true;
    }

    if (strcmp(text, "jsonl") == 0) {
        *format = TELEMETRY_JSON_LINES;
        return true;
    }

    if (strcmp(text, "csv") == 0) {
        *format = TELEMETRY_CSV;
        return true;
    }

    return false;
}

const char *telemetry_format_name(TelemetryFormat format)
{
    switch (format) {
        case TELEMETRY_TABLE:
            return "table";
        case TELEMETRY_JSON_LINES:
            return "jsonl";
        case TELEMETRY_CSV:
            return "csv";
        default:
            return "unknown";
    }
}

bool telemetry_write_header(FILE *stream, TelemetryFormat format)
{
    int result;

    if (format == TELEMETRY_TABLE) {
        result = fprintf(
            stream,
            " time | target | measured | pwm duty | controller          | machine  | fault\n"
            "======+========+==========+==========+=====================+==========+============================\n"
        );
    } else if (format == TELEMETRY_CSV) {
        result = fprintf(
            stream,
            "timestamp_ms,target_rpm,measured_rpm,pwm_duty,controller_status,machine_status,fault_code,fault_message\n"
        );
    } else if (format == TELEMETRY_JSON_LINES) {
        return true;
    } else {
        return false;
    }

    return result >= 0;
}

bool telemetry_write_record(
    FILE *stream,
    TelemetryFormat format,
    const TelemetryRecord *record
)
{
    int result;
    char measured_rpm[32];

    if (record == NULL
        || record->fault_message == NULL
        || !isfinite(record->target_rpm)
        || !isfinite(record->measured_rpm)
        || !isfinite(record->pwm_duty)) {
        return false;
    }

    if (format == TELEMETRY_TABLE) {
        if (record->measured_rpm_valid) {
            result = snprintf(
                measured_rpm,
                sizeof(measured_rpm),
                "%.0f",
                (double)record->measured_rpm
            );
            if (result < 0 || (size_t)result >= sizeof(measured_rpm)) {
                return false;
            }
        } else {
            strcpy(measured_rpm, "--");
        }

        result = fprintf(
            stream,
            "%5.1f | %6.0f | %8s | %8.1f | %-19s | %-8s | %s\n",
            (double)record->timestamp_ms / 1000.0,
            (double)record->target_rpm,
            measured_rpm,
            (double)record->pwm_duty,
            controller_status_name(record->controller_status),
            machine_status_name(record->machine_status),
            fault_code_name(record->fault_code)
        );
    } else if (format == TELEMETRY_JSON_LINES) {
        result = fprintf(
            stream,
            "{\"timestamp_ms\":%lu,\"target_rpm\":%.1f,\"measured_rpm\":",
            (unsigned long)record->timestamp_ms,
            (double)record->target_rpm
        );
        if (result < 0
            || (record->measured_rpm_valid
                ? fprintf(stream, "%.1f", (double)record->measured_rpm) < 0
                : fputs("null", stream) < 0)
            || fprintf(stream, ",\"pwm_duty\":%.1f,\"controller_status\":", (double)record->pwm_duty) < 0
            || !write_json_string(stream, controller_status_name(record->controller_status))
            || fputs(",\"machine_status\":", stream) < 0
            || !write_json_string(stream, machine_status_name(record->machine_status))
            || fputs(",\"fault_code\":", stream) < 0
            || !write_json_string(stream, fault_code_name(record->fault_code))
            || fputs(",\"fault_message\":", stream) < 0
            || !write_json_string(stream, record->fault_message)
            || fputs("}\n", stream) < 0) {
            return false;
        }
        return true;
    } else if (format == TELEMETRY_CSV) {
        result = fprintf(
            stream,
            "%lu,%.1f,",
            (unsigned long)record->timestamp_ms,
            (double)record->target_rpm
        );
        if (result < 0
            || (record->measured_rpm_valid
                && fprintf(stream, "%.1f", (double)record->measured_rpm) < 0)
            || fprintf(stream, ",%.1f,", (double)record->pwm_duty) < 0
            || !write_csv_string(stream, controller_status_name(record->controller_status))
            || fputc(',', stream) == EOF
            || !write_csv_string(stream, machine_status_name(record->machine_status))
            || fputc(',', stream) == EOF
            || !write_csv_string(stream, fault_code_name(record->fault_code))
            || fputc(',', stream) == EOF
            || !write_csv_string(stream, record->fault_message)
            || fputc('\n', stream) == EOF) {
            return false;
        }
        return true;
    } else {
        return false;
    }

    return result >= 0;
}
