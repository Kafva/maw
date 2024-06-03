#!/usr/bin/env bash
set -e

./scripts/gendata.rb

TESTS=1 make clean all

./build/maw_test -- $@

echo $? 
