CC                := clang

SRC               := $(CURDIR)/src/main.m
BIN               := $(CURDIR)/bin
PROGRAM           := av

CFLAGS            += -Wall
CFLAGS            += -Wextra
CFLAGS            += -Werror
CFLAGS            += -mmacosx-version-min=14.0
CFLAGS            += -framework Foundation
CFLAGS            += -framework AVFoundation
CFLAGS            += -fsanitize=address


all: $(BIN)/$(PROGRAM)


$(BIN)/$(PROGRAM): $(SRC)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BIN)
