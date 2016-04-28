CC     := gcc
CFLAGS := -Wall -Werror -Wfatal-errors -std=gnu11 -g -O0 -pthread -I ./common

# Subdirectory definitions
CLIENT := client
SERVER := server
COMMON := common
BUILD  := build

# Different target prefixes
LAB5_PREFIX := lab5
LAB6_SIMPLE_PREFIX := simple
LAB6_STRESS_PREFIX := stress

# Targets
LAB5_CLIENT_TARGET        := $(LAB5_PREFIX)_$(CLIENT)
LAB5_SERVER_TARGET        := $(LAB5_PREFIX)_$(SERVER)

LAB6_SIMPLE_CLIENT_TARGET := $(LAB6_SIMPLE_PREFIX)_$(CLIENT)
LAB6_SIMPLE_SERVER_TARGET := $(LAB6_SIMPLE_PREFIX)_$(SERVER)

LAB6_STRESS_CLIENT_TARGET := $(LAB6_STRESS_PREFIX)_$(CLIENT)
LAB6_STRESS_SERVER_TARGET := $(LAB6_STRESS_PREFIX)_$(SERVER)

simple: $(LAB6_SIMPLE_SERVER_TARGET) $(LAB6_SIMPLE_CLIENT_TARGET)
stress: $(LAB6_STRESS_SERVER_TARGET) $(LAB6_STRESS_CLIENT_TARGET)
all:    $(LAB5_SERVER_TARGET) $(LAB5_CLIENT_TARGET)

# Source searching
APP_SRC    := $(shell find -type f -name "app_*.c")
CLIENT_SRC := $(shell find $(CLIENT)/* -type f -name "*.c" -not -name "app_*.c")
SERVER_SRC := $(shell find $(SERVER)/* -type f -name "*.c" -not -name "app_*.c")
COMMON_SRC := $(shell find $(COMMON)/* -type f -name "*.c")

# Objects
APP_OBJ    := $(APP_OBJ:%.c=$(BUILD)%.o)
CLIENT_OBJ := $(CLIENT_SRC:%.c=$(BUILD)/%.o)
SERVER_OBJ := $(SERVER_SRC:%.c=$(BUILD)/%.o)
COMMON_OBJ := $(COMMON_SRC:%.c=$(BUILD)/%.o)

# Header dependencies
CLIENT_DEP := $(CLIENT_SRC:%.c=$(BUILD)/%.d)
SERVER_DEP := $(SERVER_SRC:%.c=$(BUILD)/%.d)
COMMON_DEP := $(COMMON_SRC:%.c=$(BUILD)/%.d)

-include $(CLIENT_DEP)
-include $(SERVER_DEP)
-include $(COMMON_DEP)

# Colors
green  := "\033[0;32m"
orange := "\033[1;33m"
end    := "\033[0m"

$(LAB6_SIMPLE_CLIENT_TARGET): $(CLIENT_OBJ) $(COMMON_OBJ) $(BUILD)/$(CLIENT)/app_simple_client.o
	@echo $(orange)+ build $@$(end)
	@$(CC) $(CFLAGS) -o $@ $^

$(LAB6_SIMPLE_SERVER_TARGET): $(SERVER_OBJ) $(COMMON_OBJ) $(BUILD)/$(SERVER)/app_simple_server.o
	@echo $(orange)+ build $@$(end)
	@$(CC) $(CFLAGS) -o $@ $^

$(LAB6_STRESS_CLIENT_TARGET): $(CLIENT_OBJ) $(COMMON_OBJ) $(BUILD)/$(CLIENT)/app_stress_client.o
	@echo $(orange)+ build $@$(end)
	@$(CC) $(CFLAGS) -o $@ $^

$(LAB6_STRESS_SERVER_TARGET): $(SERVER_OBJ) $(COMMON_OBJ) $(BUILD)/$(SERVER)/app_stress_server.o
	@echo $(orange)+ build $@$(end)
	@$(CC) $(CFLAGS) -o $@ $^

$(LAB5_CLIENT_TARGET): $(CLIENT_OBJ) $(COMMON_OBJ) $(BUILD)/$(CLIENT)/app_client.o
	@echo $(orange)+ build $@$(end)
	@$(CC) $(CFLAGS) -o $@ $^

$(LAB5_SERVER_TARGET): $(SERVER_OBJ) $(COMMON_OBJ) $(BUILD)/$(SERVER)/app_server.o
	@echo $(orange)+ build $@$(end)
	@$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/%.o: %.c Makefile
	@mkdir -p $(BUILD)/$(dir $<)
	@echo $(green)+ compile $<$(end)
	@$(CC) $(CFLAGS) -MMD -c -o $@ $<

clean:
	@rm -rf $(BUILD)
	@rm -rf $(LAB5_SERVER_TARGET)
	@rm -rf $(LAB5_CLIENT_TARGET)
	@rm -rf $(LAB6_SIMPLE_SERVER_TARGET)
	@rm -rf $(LAB6_SIMPLE_CLIENT_TARGET)
	@rm -rf $(LAB6_STRESS_SERVER_TARGET)
	@rm -rf $(LAB6_STRESS_CLIENT_TARGET)
