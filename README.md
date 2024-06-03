# maw
Music library manager

```bash
# Build
brew install libyaml
make
./build/maw --help

# Run all tests
./scripts/check.sh

# Debug a specific test case
./scripts/gendata.rb && 
    DEBUG=1 TESTS=1 make && 
    lldb -o run ./build/maw_test -- -v -l quiet -m $TESTCASE
```
