#!/usr/bin/env bash
set -e

die() { printf "$1\n" >&2 && exit 1; }

if [ "$(uname)" = Darwin ]; then
    [ -d "/opt/homebrew/opt/llvm" ] ||
     die "System clang does not work well with sanitizers: brew install llvm"
    
    # https://stackoverflow.com/a/70209891/9033629
    export MallocNanoZone=0
    export ASAN_OPTIONS=detect_leaks=1
    export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
fi

./scripts/gendata.rb 

export ASAN=1 
export DEBUG=${DEBUG:-1}
export TESTS=${TESTS:-1}
make clean all

./build/maw_test $@
