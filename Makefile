CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?= -Iinclude

BUILD_DIR := build
APP := $(BUILD_DIR)/motor_control
TEST := $(BUILD_DIR)/test_motor_control
HEADERS := $(wildcard include/*.h)
CORE_SOURCES := \
	src/machine_monitor.c \
	src/motor_controller.c \
	src/motor_model.c \
	src/scenario.c \
	src/simulated_hal.c \
	src/simulation.c \
	src/telemetry.c
APP_SOURCES := \
	src/main.c \
	$(CORE_SOURCES)
TEST_SOURCES := \
	tests/test_motor_control.c \
	$(CORE_SOURCES)
DASHBOARD_DATA := \
	docs/data/normal.jsonl \
	docs/data/sensor-disconnect.jsonl \
	docs/data/mechanical-jam.jsonl

.PHONY: all run test dashboard-data clean

all: $(APP)

$(APP): $(APP_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(APP_SOURCES) -o $(APP)

$(TEST): $(TEST_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_SOURCES) -o $(TEST)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(APP)
	./$(APP)

test: $(TEST)
	./$(TEST)

dashboard-data: $(APP)
	./$(APP) --scenario normal --format jsonl > docs/data/normal.jsonl
	./$(APP) --scenario sensor-disconnect --format jsonl > docs/data/sensor-disconnect.jsonl
	./$(APP) --scenario mechanical-jam --format jsonl > docs/data/mechanical-jam.jsonl

clean:
	rm -rf $(BUILD_DIR)
