PHONY   := all test clean valgrind

PROJECT  := sighook
LIB_NAME := lib$(PROJECT).a
TEST_BIN := test_$(PROJECT)

INC_DIR   := include
SRC_DIR   := src
TEST_DIR  := tests
OBJ_DIR   := obj

LIB_SRCS  := $(shell find $(SRC_DIR) -name '*.c' -or -name '*.s')
TEST_SRCS := $(shell find $(TEST_DIR) -name '*.c' -or -name '*.s')

LIB_OBJS  := $(LIB_SRCS:%=$(OBJ_DIR)/%.o)
TEST_OBJS := $(TEST_SRCS:%=$(OBJ_DIR)/%.o)

CC       := gcc
AS       := as
AR       := ar
WARNINGS := -Wall -Wextra -Wpedantic
CFLAGS   := $(WARNINGS) -std=c11 -I$(INC_DIR) -O2 -fPIC
LDFLAGS  := -flto

RM    := rm -f
RMF   := rm -rf
RMDIR := rmdir

all: $(LIB_NAME) $(TEST_BIN)

$(LIB_NAME): $(LIB_OBJS)
	@echo "[AR]\t$@"
	@$(AR) rcs $@ $^

$(TEST_BIN): $(TEST_OBJS) $(LIB_NAME)
	@echo "[LD]\t$@"
	@$(CC) $(TEST_OBJS) -L. -l$(PROJECT) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.c.o: %.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	@echo "[CC]\t$@"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.s.o: %.s | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	@echo "[AS]\t$@"
	@$(AS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $@

test: $(TEST_BIN)
	@./$(TEST_BIN)

clean:
	@$(RMF) $(OBJ_DIR) $(LIB_NAME) $(TEST_BIN)

valgrind: $(PROJECT)
	@valgrind ./$(TEST_BIN)

