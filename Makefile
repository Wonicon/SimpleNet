CC     := gcc
CFLAGS := -Wall -Werror -Wfatal-errors -std=gnu11 -g -O0 -pthread -I ./common

PREFIX := lab5

# Subdirectory definitions
CLIENT := client
SERVER := server
COMMON := common
BUILD  := build

# Targets
CLIENT_TARGET := $(PREFIX)_$(CLIENT)
SERVER_TARGET := $(PREFIX)_$(SERVER)

all: $(SERVER_TARGET) $(CLIENT_TARGET)

client: $(CLIENT_TARGET)

server: $(SERVER_TARGET)

# Source searching
CLIENT_SRC := $(shell find $(CLIENT)/* -type f -name "*.c")
SERVER_SRC := $(shell find $(SERVER)/* -type f -name "*.c")
COMMON_SRC := $(shell find $(COMMON)/* -type f -name "*.c")

# Objects
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

$(CLIENT_TARGET): $(CLIENT_OBJ) $(COMMON_OBJ)
	@echo $(orange)+ build $@$(end)
	@$(CC) $(CFLAGS) -o $@ $^

$(SERVER_TARGET): $(SERVER_OBJ) $(COMMON_OBJ)
	@echo $(orange)+ build $@$(end)
	@$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/%.o: %.c Makefile
	@mkdir -p $(BUILD)/$(dir $<)
	@echo $(green)+ compile $<$(end)
	@$(CC) $(CFLAGS) -MMD -c -o $@ $<

clean:
	@rm -rf $(BUILD)
	@rm -rf $(SERVER_TARGET)
	@rm -rf $(CLIENT_TARGET)
