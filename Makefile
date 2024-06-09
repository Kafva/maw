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
BUILD             = $(CURDIR)/build

CFLAGS            += -DMAW_PROGRAM=\"$(PROGRAM)\"
CFLAGS            += -std=c99
CFLAGS            += -fstack-protector-all
ifeq ($(UNAME),linux)
CFLAGS            += -D_GNU_SOURCE
endif
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
LDFLAGS           += -lyaml
ifeq ($(STATIC),1)
# Statically link maw against its dependencies if set.
# This can be useful for debugging, the static libs are compiled
# with debug symbols by default.
ifeq ($(UNAME),darwin)
LDFLAGS           += -framework Foundation
LDFLAGS           += -framework AppKit
LDFLAGS           += -framework Security
LDFLAGS           += -framework Metal
LDFLAGS           += -framework OpenGL
LDFLAGS           += -framework CoreFoundation
LDFLAGS           += -framework CoreVideo
LDFLAGS           += -framework CoreImage
LDFLAGS           += -framework CoreMedia
LDFLAGS           += -framework CoreGraphics
LDFLAGS           += -framework AudioToolbox
LDFLAGS           += -framework VideoToolbox
LDFLAGS           += -framework AVFoundation
LDFLAGS           += -lz
LDFLAGS           += -liconv
LDFLAGS           += -lbz2
else
LDFLAGS           += -static
endif # UNAME
LDFLAGS           += $(BUILD)/deps/lib/libavcodec.a
LDFLAGS           += $(BUILD)/deps/lib/libavformat.a
LDFLAGS           += $(BUILD)/deps/lib/libavutil.a
LDFLAGS           += $(BUILD)/deps/lib/libavfilter.a
LDFLAGS           += $(BUILD)/deps/lib/libswresample.a
LDFLAGS           += $(BUILD)/deps/lib/libswscale.a
LDFLAGS           += $(BUILD)/deps/lib/libavdevice.a
else
LDFLAGS           += -lavcodec
LDFLAGS           += -lavformat
LDFLAGS           += -lavutil
LDFLAGS           += -lavfilter
endif # STATIC


# Release/debug only flags
ifeq ($(DEBUG),1)
CFLAGS            += -g
CFLAGS            += -O0
CFLAGS            += -fsanitize=address
CFLAGS            += -Wno-unused
CFLAGS            += -Wno-unused-parameter
else
CFLAGS            += -O3
CFLAGS            += -Wunreachable-code
CFLAGS            += -Wunused
endif

ifeq ($(STATIC),1)
all: dep
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

################################################################################

# (Optional) Download and build dependencies from source
dep: $(BUILD)/deps/lib/libavfilter.a $(BUILD)/deps/lib/libyaml.a

$(BUILD)/deps/lib/libavfilter.a: $(CURDIR)/deps/FFmpeg
	mkdir -p $(dir $@)
	(cd $< && ./configure --prefix=$(BUILD)/deps --enable-debug)
	$(MAKE) -C $<
	$(MAKE) -C $< install

$(BUILD)/deps/lib/libyaml.a: $(CURDIR)/deps/libyaml
	(cd $< && autoreconf -vfi)
	(cd $< && ./configure --prefix=$(BUILD)/deps --enable-debug)
	$(MAKE) -C $<
	$(MAKE) -C $< install

$(CURDIR)/deps/FFmpeg:
	mkdir -p $(dir $@)
	git clone --depth 1 https://github.com/FFmpeg/FFmpeg.git $@

$(CURDIR)/deps/libyaml:
	mkdir -p $(dir $@)
	git clone --depth 1 https://github.com/yaml/libyaml.git $@

clean:
	rm -rf $(BUILD)/*.o $(BUILD)/.*.json $(BUILD)/tests $(BUILD)/maw*

distclean:
	rm -rf $(BUILD)
