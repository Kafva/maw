CC                := clang
CFLAGS            := -Wall -Wextra -Werror -mmacosx-version-min=14.0 -framework AVFoundation

SRC               := $(CURDIR)/src/main.m
OUT               := $(CURDIR)/target
OUT_PLATFORM      := $(OUT)/$(shell uname -m)

all: $(OUT_PLATFORM)/av


$(OUT_PLATFORM)/av: $(SRC)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(OUT)
