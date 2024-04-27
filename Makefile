CC                := clang
CFLAGS            := -Wall -Wextra -Werror -mmacosx-version-min=14.0 -framework AVFoundation

SRC               := $(CURDIR)/src/main.m
BIN               := $(CURDIR)/bin

all: $(BIN)/av


$(BIN)/av: $(SRC)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BIN)
