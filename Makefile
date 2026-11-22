# SETTINGS
CC := gcc
CFLAGS := -Wall -Wextra -g -O0 -Iinclude -Isrc -fsanitize=address,undefined -MMD -MP
LDFLAGS := -fsanitize=address,undefined
LDLIBS := -lsodium

PROG_DIR := playground

# SOURCES
SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:src/%.c=build/%.o)

PROGS := $(patsubst $(PROG_DIR)/%.c,build/%,$(wildcard $(PROG_DIR)/*.c))

# RULES
all: $(PROGS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%: $(PROG_DIR)/%.c $(OBJS) | build
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(OBJS) $(LDLIBS) -o $@

build:
	mkdir -p build

clean:
	rm -rf build

# DEPENDENCIES
-include $(wildcard build/*.d)

.PHONY: all clean

.SECONDARY: