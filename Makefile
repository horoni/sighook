.PHONY   := all test clean valgrind

PROJECT   := sighook
LIB_NAME  := lib$(PROJECT).a

INC_DIR   := include
SRC_DIR   := src
TEST_DIR  := tests
OBJ_DIR   := obj

LIB_SRCS  := $(shell find $(SRC_DIR) -name '*.c' -or -name '*.s')
LIB_OBJS  := $(LIB_SRCS:%=$(OBJ_DIR)/%.o)

TEST_SRCS := $(wildcard $(TEST_DIR)/test_*.c)
TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(TEST_DIR)/%,$(TEST_SRCS))

CC       := gcc
AS       := as
AR       := ar
WARNINGS := -Wall -Wextra -Wpedantic
CFLAGS   := $(WARNINGS) -std=c11 -I$(INC_DIR) -O2 -fPIC
LDFLAGS  := -flto

RM    := rm -f
RMF   := rm -rf
RMDIR := rmdir

all: $(LIB_NAME) $(TEST_BINS)

$(LIB_NAME): $(LIB_OBJS)
	@echo "[AR]\t$@"
	@$(AR) rcs $@ $^

$(TEST_DIR)/test_%: $(OBJ_DIR)/$(TEST_DIR)/test_%.c.o $(LIB_NAME)
	@echo "[LD]\t$@"
	@$(CC) $^ -o $@ $(LDFLAGS)

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

test: $(TEST_BINS)
	@for bin in $(TEST_BINS); do \
		echo "Start $$bin..."; \
		./$$bin || exit 1; \
	done

clean:
	@$(RMF) $(OBJ_DIR) $(LIB_NAME) $(TEST_BINS)

valgrind: $(TEST_BINS)
	@for bin in $(TEST_BINS); do \
		echo "Valgrind: $$bin"; \
		valgrind --leak-check=full ./$$bin || exit 1; \
	done

