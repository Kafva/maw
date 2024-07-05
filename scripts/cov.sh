#!/usr/bin/env bash
set -e
export DEBUG=1
export TESTS=1
export COVERAGE=1
export LLVM_PROFILE_FILE=build/maw.profraw
PROFDATA=build/maw.profdata
PROGRAM=build/maw_test

make clean all

$PROGRAM || :

llvm-profdata merge -sparse $LLVM_PROFILE_FILE -o $PROFDATA

llvm-cov report $PROGRAM -instr-profile=$PROFDATA
