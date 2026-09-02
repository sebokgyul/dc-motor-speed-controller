CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?= -Iinclude

BUILD_DIR := build
APP := $(BUILD_DIR)/motor_control
HEADERS := $(wildcard include/*.h)
APP_SOURCES := \
	src/main.c \
	src/motor_controller.c \
	src/motor_model.c \
	src/simulated_hal.c

.PHONY: all run clean

all: $(APP)

$(APP): $(APP_SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(APP_SOURCES) -o $(APP)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: $(APP)
	./$(APP)

clean:
	rm -rf $(BUILD_DIR)
