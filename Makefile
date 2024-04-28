CC                := clang

SRC               := $(wildcard $(CURDIR)/src/*.m)
BUILD               := $(CURDIR)/bin
PROGRAM           := av

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
# Santitizers
CFLAGS            += -pedantic
CFLAGS            += -fsanitize=address
CFLAGS            += -fno-common
CFLAGS            += -fstack-protector-all
# Generate compilation database
CFLAGS            += -MJ .compile_commands.json.intermediate
# Libraries
CFLAGS            += -mmacosx-version-min=14.0
CFLAGS            += -framework Foundation
CFLAGS            += -framework AVFoundation
CFLAGS            += -lyaml

all: $(BUILD)/$(PROGRAM) compile_commands.json


$(BUILD)/$(PROGRAM): $(SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

compile_commands.json: $(BUILD)/$(PROGRAM)
	@echo [ > $@
	@cat .compile_commands.json.intermediate >> $@
	@echo ] >> $@
	@rm .compile_commands.json.intermediate

test:
	$(CURDIR)/tests/run

clean:
	rm -rf $(BUILD) $(CURDIR)/tests/music
