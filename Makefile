CC                := clang
UNAME 			  := $(shell uname -s | tr '[:upper:]' '[:lower:]')

SRCS              = $(wildcard src/*.c)
TEST_SRCS         = $(wildcard tests/*.c)
HEADERS           = $(wildcard src/*.h)
OBJS              = $(SRCS:src/%.c=$(BUILD)/%.o)
BUILD             = build
PROGRAM           = maw

CFLAGS            += -std=c99
# Includes
CFLAGS            += -I$(CURDIR)/src
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
CFLAGS            += -pedantic
# Libraries
LDFLAGS           += -lavcodec
LDFLAGS           += -lavformat
LDFLAGS           += -lavutil
LDFLAGS           += -lyaml

# Release/debug only flags
ifeq ($(DEBUG),1)
CFLAGS            += -g
CFLAGS            += -O1
CFLAGS            += -fsanitize=address
CFLAGS            += -fstack-protector-all
CFLAGS            += -Wno-unused
else
CFLAGS            += -O3
CFLAGS            += -Wunreachable-code
CFLAGS            += -Wunused
endif


all: $(BUILD)/$(PROGRAM) compile_commands.json

$(BUILD)/%.o: $(CURDIR)/src/%.c
	@mkdir -p $(dir $@)
	@# A compilation database for each TU is created with `-MJ`
	$(CC) $(CFLAGS) -MJ $(dir $@)/.$(notdir $@).json $< -c -o $@

$(BUILD)/$(PROGRAM): $(OBJS) $(HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

compile_commands.json: $(BUILD)/$(PROGRAM)
	@# Combine JSON fragments from build into a complete compilation database
	@echo [ > $@
	@cat $(BUILD)/.*.json >> $@
	@echo ] >> $@

clean:
	rm -rf $(BUILD) $(CURDIR)/tests/music
