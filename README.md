# maw
Music library manager

```bash
# Install dependences
brew install libyaml ffmpeg

# Build options:
#   DEBUG=1:    Build with debug configuration
#   TESTS=1:    Build unit tests executable
#   STATIC=1:   Build dependences from source (with debug symbols) 
#               and link statically
make
./build/maw --help


# Run all tests
./scripts/check.sh

# Debug a specific test case
./scripts/gendata.rb && 
    STATIC=1 DEBUG=1 TESTS=1 make && 
    lldb -o run ./build/maw_test -- -v -l quiet -m $TESTCASE
```
