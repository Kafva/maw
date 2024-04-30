CC                := clang

SRCS              = $(wildcard src/*.c)
OBJS              = $(SRCS:src/%.c=$(BUILD)/%.o)
BUILD             = build
PROGRAM           = maw

# XXX
CFLAGS            += -g
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
LDFLAGS            += -lavformat
LDFLAGS            += -lavutil
LDFLAGS            += -lyaml

all: $(BUILD)/$(PROGRAM) compile_commands.json

$(BUILD)/%.o: $(CURDIR)/src/%.c
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
