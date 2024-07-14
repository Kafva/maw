# maw
Maw provides a way to declaratively configure metadata for mp4/m4a files based
on a config. See example configuration below, read by default from
`~/.config/maw/maw.yml`:

```yaml
# Directory with custom thumbnail artwork
art_dir: ~/Pictures/art
# Directory with mp4 files
music_dir: ~/Music

# Each entry under `metadata` corresponds to a path (or glob expression)
# relative to `music_dir`
metadata:
  # A directory entry will match all files underneath it
  red:
    # The following metadata fields can be configured:
    #   title
    #   album
    #   artist
    # The title defaults to the filename (without extension) for each file.
    album: Red album
    artist: Red artist
    # The 'cover' field accepts a path relative to `art_dir` with thumbnail artwork to set or
    # one of these flags:
    #   KEEP: keep the current artwork if any [default]
    #   CLEAR: remove artwork if any
    #   CROP: crop artwork from 1280x720 -> 720x720
    cover: red.png
    # Set to true to clear all other metadata fields, e.g. composer etc.
    clean: true
  # This entry also matches files under `red/`, matches later in the configuration
  # take precedence!
  red/*no_cover.m4a:
    cover: CLEAR

playlists:
  # Define a .m3u playlist with the files below, it will land in
  # ~/Music/.playlist1.m3u, folders and glob expressions are allowed
  playlist1:
    - red/track01.m4a
    - blue/very blue*.m4a
    - yellow
```

To apply the configuration:
```bash
maw update
```

A limited set of the configuration can also be applied:
```bash
# Only includes paths under 'red' from the configuration
maw update red
```

To generate the playlists defined in the YAML configuration
```bash
maw generate
```

Maw is purposefully made to only produce a specific type of output. The
output files will always have one audio stream and optionally one video stream
with cover data. Subtitle streams etc. in the input file are always removed.

## Building

```bash
# macOS
brew install libyaml ffmpeg nasm
make install

# Debian/Ubuntu
# Requires ffmpeg version 6.1.1 or newer
sudo apt install clang \
                 ffmpeg \
                 libyaml-dev \
                 libavformat-dev \
                 libavfilter-dev \
                 libavutil-dev \
                 libavcodec-dev \
                 libswresample-dev \
                 libswscale-dev
make install

# NixOS
# Build and run directly
nix run "github:Kafva/maw"

# Development shell
nix develop -c $SHELL
```

## Development notes
```bash
# Build options:
#   DEBUG=1:      Build with debug configuration, dependencies are also built
#                 from source with debug symbols when this option is enabled
#   ASAN=1        Build with LLVM AddressSanitizer
#   TESTS=1:      Build unit tests executable
#   COVERAGE=1    Build with coverage instrumentation flags
make
./build/maw --help

# Run all tests (check for regressions)
make test

# Run all tests with coverage
make coverage

# Check for memory leaks with AddressSanitizer
./scripts/asan.sh -m $TESTCASE

# Debug a specific test case
./scripts/gendata.rb && DEBUG=1 TESTS=1 make &&
    lldb -o run ./build/maw_test -- -v -l quiet -m $TESTCASE
```
