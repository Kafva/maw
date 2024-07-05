# maw
Music library manager

```bash
# Install dependences
brew install libyaml ffmpeg nasm

# Build options:
#   DEBUG=1:    Build with debug configuration, dependencies are also built
#               from source with debug symbols when this option is enabled
#   TESTS=1:    Build unit tests executable
#   COVERAGE=1  Build with coverage instrumentation flags
make
./build/maw --help


# Run all tests
./scripts/check.sh

# Debug a specific test case
./scripts/gendata.rb &&
    DEBUG=1 TESTS=1 make &&
    lldb -o run ./build/maw_test -- -v -l quiet -m $TESTCASE

# Coverage
scripts/cov.sh
```
