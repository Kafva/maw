CC                := clang
PREFIX            ?= $(HOME)/.local
UNAME 			  := $(shell uname -s | tr '[:upper:]' '[:lower:]')

SRCS              = $(wildcard src/*.c)
HEADERS           = $(wildcard include/*.h)

# Tests
PROGRAM_TEST      = maw_test
LLVM_PROFDATA     = $(BUILD)/$(PROGRAM_TEST).profdata
LLVM_PROFILE_FILE =  $(BUILD)/$(PROGRAM_TEST).profraw

ifeq ($(TESTS),1)
CFLAGS            += -DMAW_TEST
SRCS              += $(wildcard src/tests/*.c)
HEADERS           += $(wildcard include/tests/*.h)
SRCS_PATTERN      = $(CURDIR)/src/**%.c
PROGRAM           = $(PROGRAM_TEST)
else
SRCS_PATTERN      = $(CURDIR)/src/%.c
PROGRAM           = maw
endif

OBJS              = $(SRCS:src/%.c=$(BUILD)/%.o)
BUILD             = $(CURDIR)/build

CFLAGS            += -DMAW_PROGRAM=\"$(PROGRAM)\"
CFLAGS            += -DMAW_VERSION=\"$(shell git describe --tags)\"
CFLAGS            += -DMAW_BUILDER_NAME=\"$(shell git config user.name)\"
CFLAGS            += -DMAW_BUILDER_EMAIL=\"$(shell git config user.email)\"
CFLAGS            += -std=c99
CFLAGS            += -pthread
ifeq ($(UNAME),linux)
CFLAGS            += -D_GNU_SOURCE
CFLAGS            += -D_FORTIFY_SOURCE=2
endif
CFLAGS            += -fstack-protector-all
CFLAGS            += -I$(CURDIR)/include

# Warnings
CFLAGS            += -pedantic
CFLAGS            += -Wall
CFLAGS            += -Wextra
CFLAGS            += -Werror
CFLAGS            += -Wstrict-prototypes
CFLAGS            += -Wmissing-prototypes
CFLAGS            += -Wmissing-declarations
CFLAGS            += -Wimplicit-fallthrough
CFLAGS            += -Wshadow
CFLAGS            += -Wpointer-arith
CFLAGS            += -Wcast-qual
CFLAGS            += -Wsign-compare
CFLAGS            += -Wtype-limits
CFLAGS            += -Wdeclaration-after-statement
CFLAGS            += -Wformat
CFLAGS            += -Wuninitialized
CFLAGS            += -Wconversion
CFLAGS            += -Wnull-dereference

# Libraries
LDFLAGS           += -lyaml
LDFLAGS           += -lavcodec
LDFLAGS           += -lavformat
LDFLAGS           += -lavutil
LDFLAGS           += -lavfilter
ifeq ($(UNAME),linux)
LDFLAGS           += -lswresample
LDFLAGS           += -lswscale
endif

# Release/debug only flags
ifeq ($(DEBUG),1)
CFLAGS            += -g
ifeq ($(UNAME),darwin)
CFLAGS            += -O0
else
CFLAGS            += -Og
endif
CFLAGS            += -Wno-unused
CFLAGS            += -Wno-unused-parameter
# Use the libraries that we build from source
CFLAGS            += -I$(BUILD)/deps/include
LDFLAGS           += -Wl,-rpath,$(BUILD)/deps/lib
LDFLAGS           += -L$(BUILD)/deps/lib
all: dep
else
CFLAGS            += -O3
CFLAGS            += -Wunreachable-code
CFLAGS            += -Wunused
ifeq ($(UNAME),darwin)
CFLAGS            += -I/opt/homebrew/include
endif # UNAME
endif # DEBUG

# XXX: This needs to disabled when using valgrind
ifeq ($(ASAN),1)
CFLAGS            += -fsanitize=address
ifeq ($(UNAME),darwin)
# Use newer llvm with "support" for leak sanitizer
# https://discourse.llvm.org/t/does-leaksanitizer-not-work-on-macos-13-apple-silicon/73148/2
LDFLAGS           += -Wl,-rpath,/opt/homebrew/opt/llvm/lib
LDFLAGS           += -L/opt/homebrew/opt/llvm/lib
CC                := /opt/homebrew/opt/llvm/bin/clang
endif # UNAME
endif # ASAN

ifeq ($(COVERAGE),1)
COVERAGE_FLAGS    = -fprofile-instr-generate
COVERAGE_FLAGS    += -fcoverage-mapping
CFLAGS            += $(COVERAGE_FLAGS)
LDFLAGS           += $(COVERAGE_FLAGS)
endif

all: $(BUILD)/$(PROGRAM) compile_commands.json

$(BUILD)/%.o: $(SRCS_PATTERN) $(HEADERS)
	@mkdir -p $(dir $@)
	@# A compilation database for each TU is created with `-MJ`
	$(CC) $(CFLAGS) -MJ $(dir $@)/.$(notdir $@).json $< -c -o $@

$(BUILD)/$(PROGRAM): $(OBJS)
	@$(CC) --version
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

compile_commands.json: $(BUILD)/$(PROGRAM)
	@# Combine JSON fragments from build into a complete compilation database
	@echo [ > $@
	@cat $(BUILD)/.*.json >> $@
	@echo ] >> $@

# For DEBUG builds, download and build dependencies from source
dep: $(BUILD)/deps/lib/libavfilter.a $(BUILD)/deps/lib/libyaml.a

$(BUILD)/deps/lib/libavfilter.a: $(CURDIR)/deps/FFmpeg
	mkdir -p $(dir $@)
	cd $< && ./configure --prefix=$(BUILD)/deps \
	                     --disable-programs \
	                     --disable-stripping \
	                     --enable-debug \
	                     --enable-shared
	$(MAKE) -C $<
	$(MAKE) -C $< install

$(BUILD)/deps/lib/libyaml.a: $(CURDIR)/deps/libyaml
	cd $< && autoreconf -vfi
	cd $< && ./configure --prefix=$(BUILD)/deps
	$(MAKE) -C $<
	$(MAKE) -C $< install

$(CURDIR)/deps/FFmpeg:
	mkdir -p $(dir $@)
	git clone --depth 1 https://github.com/FFmpeg/FFmpeg.git $@

$(CURDIR)/deps/libyaml:
	mkdir -p $(dir $@)
	git clone --depth 1 https://github.com/yaml/libyaml.git $@

lint:
	clang-format -i $(shell find $(CURDIR)/src -type f -name '*.c') \
		$(shell find $(CURDIR)/include -type f -name '*.h')

test:
	$(CURDIR)/scripts/gendata.rb
	DEBUG=0 TESTS=1 $(MAKE) clean all
	$(BUILD)/$(PROGRAM_TEST)

coverage:
	$(CURDIR)/scripts/gendata.rb
	DEBUG=0 TESTS=1 COVERAGE=1 $(MAKE) clean all
	LLVM_PROFILE_FILE=$(LLVM_PROFILE_FILE) \
					  $(BUILD)/$(PROGRAM_TEST)
	llvm-profdata merge -sparse $(LLVM_PROFILE_FILE) -o $(LLVM_PROFDATA)
	llvm-cov report $(BUILD)/$(PROGRAM_TEST) \
		-instr-profile=$(LLVM_PROFDATA) \
		--ignore-filename-regex='include/*' \
		--ignore-filename-regex='deps/*'

install: $(BUILD)/$(PROGRAM)
	install -D $< $(PREFIX)/bin/$(PROGRAM)

clean:
	rm -rf $(BUILD)/*.o $(BUILD)/.*.json $(BUILD)/tests $(BUILD)/$(PROGRAM) \
		$(BUILD)/$(PROGRAM_TEST)

distclean:
	rm -rf $(BUILD)
