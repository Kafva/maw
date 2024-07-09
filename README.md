# maw
Maw provides a way to declaratively configure metadata for media files based on
a config. Example configuration, read by default from `~/.config/maw/maw.yml`:

```yaml
# Directory with custom thumbnail artwork
art_dir: ~/Pictures/art
# Directory with media files
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
    # Path relative to `art_dir` with thumbnail artwork to set
    cover: red.png
    # Should all other metadata fields, composer etc. be cleared?
    clean: true

  # This entry also matches files under `red/`, matches later in the configuration
  # take precedence!
  red/*no_cover.m4a:
    # What to do with thumbnails when no `cover` is provided?
    # Possible values CLEAR|KEEP|CROP, default is KEEP.
    # CROP will crop from 1280x720 -> 720x720
    cover_policy: CLEAR

playlists:
  # Define a .m3u playlist with the files below,
  # folders and glob expressions allowed
  playlist1:
    - red/track01.m4a
    - blue/track01.m4a
    - yellow
```

To apply the configuration:
```bash
maw -c maw.yml update
```

A limited set of the configuration can also be applied:
```bash
# Only includes paths under 'red' from the configuration
maw -c maw.yml update red
```

To generate playlists defined in the YAML configuration
```bash
maw -c maw.yml generate
```

## Building

### macOS
```bash
brew install libyaml ffmpeg nasm
make install
```

### NixOS
```bash
nix build
```

## Development notes
```bash
# Build options:
#   DEBUG=1:    Build with debug configuration, dependencies are also built
#               from source with debug symbols when this option is enabled
#   TESTS=1:    Build unit tests executable
#   COVERAGE=1  Build with coverage instrumentation flags
make
./build/maw --help

# XXX: When building DEBUG on macOS, do not use the system clang.
# The system default version does not work well with sanitizers.
brew install llvm
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"

# Run all tests
make test

# Run all tests with coverage
make coverage

# Debug a specific test case
./scripts/gendata.rb && DEBUG=1 TESTS=1 make &&
    lldb -o run ./build/maw_test -- -v -l quiet -m $TESTCASE

# Check for memory leaks on macOS
# https://stackoverflow.com/a/70209891/9033629
export MallocNanoZone=0
export ASAN_OPTIONS=detect_leaks=1

./scripts/gendata.rb && DEBUG=1 TESTS=1 make &&
    ./build/maw_test -v -l quiet -m $TESTCASE
```
