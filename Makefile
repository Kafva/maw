CC                := clang
UNAME 			  := $(shell uname -s | tr '[:upper:]' '[:lower:]')

SRCS              = $(wildcard src/*.c)
HEADERS           = $(wildcard include/*.h)

ifeq ($(TESTS),1)
CFLAGS            += -DMAW_TEST
SRCS              += $(wildcard src/tests/*.c)
HEADERS           += $(wildcard include/tests/*.h)
SRCS_PATTERN      = $(CURDIR)/src/**%.c
PROGRAM           = maw_test
else
SRCS_PATTERN       = $(CURDIR)/src/%.c
PROGRAM           = maw
endif

OBJS              = $(SRCS:src/%.c=$(BUILD)/%.o)
BUILD             = build

CFLAGS            += -DMAW_PROGRAM=\"$(PROGRAM)\"
CFLAGS            += -std=c99
CFLAGS            += -fstack-protector-all
# Includes
CFLAGS            += -I$(CURDIR)/include
ifeq ($(UNAME),darwin)
CFLAGS            += -I/opt/homebrew/include
endif
# Warnings
CFLAGS            += -Wall
CFLAGS            += -Wextra
CFLAGS            += -Werror
CFLAGS            += -Wstrict-prototypes
CFLAGS            += -Wmissing-prototypes
CFLAGS            += -Wmissing-declarations
CFLAGS            += -Wshadow
CFLAGS            += -Wpointer-arith
CFLAGS            += -Wcast-qual
CFLAGS            += -Wsign-compare
CFLAGS            += -Wtype-limits
CFLAGS            += -Wdeclaration-after-statement
CFLAGS            += -pedantic
# Libraries
LDFLAGS           += -lavcodec
LDFLAGS           += -lavformat
LDFLAGS           += -lavutil
LDFLAGS           += -lyaml

# Release/debug only flags
ifeq ($(DEBUG),1)
CFLAGS            += -g
CFLAGS            += -Og
CFLAGS            += -fsanitize=address
CFLAGS            += -Wno-unused
CFLAGS            += -Wno-unused-parameter
else
CFLAGS            += -O3
CFLAGS            += -Wunreachable-code
CFLAGS            += -Wunused
endif

all: $(BUILD)/$(PROGRAM) compile_commands.json

$(BUILD)/%.o: $(SRCS_PATTERN) $(HEADERS)
	@mkdir -p $(dir $@)
	@# A compilation database for each TU is created with `-MJ`
	$(CC) $(CFLAGS) -MJ $(dir $@)/.$(notdir $@).json $< -c -o $@

$(BUILD)/$(PROGRAM): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

compile_commands.json: $(BUILD)/$(PROGRAM)
	@# Combine JSON fragments from build into a complete compilation database
	@echo [ > $@
	@cat $(BUILD)/.*.json >> $@
	@echo ] >> $@

clean:
	rm -rf $(BUILD)
