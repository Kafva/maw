CC                := clang

SRCS              = $(wildcard src/*.m)
OBJS              = $(SRCS:src/%.m=$(BUILD)/%.o)
BUILD             = build
PROGRAM           = maw

CFLAGS            += -mmacosx-version-min=14.0
# Includes
CFLAGS            += -I$(CURDIR)/src
CFLAGS            += -I/opt/homebrew/include
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
# Santitizers
CFLAGS            += -fsanitize=address
CFLAGS            += -fstack-protector-all
# Libraries
LDFLAGS            += -framework Foundation
LDFLAGS            += -framework AVFoundation
LDFLAGS            += -lyaml

all: $(BUILD)/$(PROGRAM) compile_commands.json

$(BUILD)/%.o: $(CURDIR)/src/%.m
	@mkdir -p $(dir $@)
	@# A compilation database for each TU is created with `-MJ`
	$(CC) $(CFLAGS) -MJ $(dir $@)/.$(notdir $@).json $< -c -o $@

$(BUILD)/$(PROGRAM): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

compile_commands.json: $(BUILD)/$(PROGRAM)
	@# Combine JSON fragments from build into a complete compilation database
	@echo [ > $@
	@cat $(BUILD)/.*.json >> $@
	@echo ] >> $@

test:
	$(CURDIR)/tests/run

clean:
	rm -rf $(BUILD) $(CURDIR)/tests/music
