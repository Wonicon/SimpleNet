CC     := gcc
CFLAGS := -Wall -Werror -Wfatal-errors -std=gnu11 -g -Og -pthread -I ./common

# Subdirectory definitions
CLIENT := client
SERVER := server
COMMON := common
BUILD  := build

# Different target prefixes
LAB5_PREFIX := lab5
LAB6_SIMPLE_PREFIX := simple
LAB6_STRESS_PREFIX := stress

LAB5_CLIENT_TARGET        := $(LAB5_PREFIX)_$(CLIENT)
LAB5_SERVER_TARGET        := $(LAB5_PREFIX)_$(SERVER)

LAB6_SIMPLE_CLIENT_TARGET := $(LAB6_SIMPLE_PREFIX)_$(CLIENT)
LAB6_SIMPLE_SERVER_TARGET := $(LAB6_SIMPLE_PREFIX)_$(SERVER)

LAB6_STRESS_CLIENT_TARGET := $(LAB6_STRESS_PREFIX)_$(CLIENT)
LAB6_STRESS_SERVER_TARGET := $(LAB6_STRESS_PREFIX)_$(SERVER)

lab07: CFLAGS += -I ./topology
lab07: sip/sip son/son

lab06: simple stress

lab05: $(LAB5_SERVER_TARGET) $(LAB5_CLIENT_TARGET)

stress: $(LAB6_STRESS_SERVER_TARGET) $(LAB6_STRESS_CLIENT_TARGET)

simple: $(LAB6_SIMPLE_SERVER_TARGET) $(LAB6_SIMPLE_CLIENT_TARGET)

APP_SRC := $(shell find -type f -name "app_*.c")
APP_OBJ := $(APP_OBJ:%.c=$(BUILD)%.o)
APP_DEP := $(APP_SRC:%.c=$(BUILD)/%.d)
-include $(APP_DEP)

CLIENT_SRC := $(shell find $(CLIENT)/* -type f -name "*.c" -not -name "app_*.c")
CLIENT_OBJ := $(CLIENT_SRC:%.c=$(BUILD)/%.o)
CLIENT_DEP := $(CLIENT_SRC:%.c=$(BUILD)/%.d)
-include $(CLIENT_DEP)

SERVER_SRC := $(shell find $(SERVER)/* -type f -name "*.c" -not -name "app_*.c")
SERVER_OBJ := $(SERVER_SRC:%.c=$(BUILD)/%.o)
SERVER_DEP := $(SERVER_SRC:%.c=$(BUILD)/%.d)
-include $(SERVER_DEP)

COMMON_SRC := $(shell find $(COMMON)/* -type f -name "*.c")
COMMON_OBJ := $(COMMON_SRC:%.c=$(BUILD)/%.o)
COMMON_DEP := $(COMMON_SRC:%.c=$(BUILD)/%.d)
-include $(COMMON_DEP)

SON_SRC := $(shell find son/* -type f -name "*.c")
SON_OBJ := $(SON_SRC:%.c=$(BUILD)/%.o)
SON_DEP := $(SON_SRC:%.c=$(BUILD)/%.d)
-include $(SON_DEP)

SIP_SRC := $(shell find sip/* -type f -name "*.c")
SIP_OBJ := $(SIP_SRC:%.c=$(BUILD)/%.o)
SIP_DEP := $(SIP_SRC:%.c=$(BUILD)/%.d)
-include $(SIP_DEP)

TOPOLOGY_SRC := $(shell find topology/* -type f -name "*.c")
TOPOLOGY_OBJ := $(TOPOLOGY_SRC:%.c=$(BUILD)/%.o)
TOPOLOGY_DEP := $(TOPOLOGY_SRC:%.c=$(BUILD)/%.d)
-include $(TOPOLOGY_DEP)

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

son/son: $(COMMON_OBJ) $(TOPOLOGY_OBJ) $(SON_OBJ)
	@echo $(organe)+ build $@$(end)
	@$(CC) $(CFLAGS) -o $@ $^

sip/sip: $(COMMON_OBJ) $(TOPOLOGY_OBJ) $(SIP_OBJ)
	@echo $(organe)+ build $@$(end)
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
	@rm -rf son/son
	@rm -rf sip/sip
