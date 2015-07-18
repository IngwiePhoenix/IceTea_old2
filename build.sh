#!/bin/bash
cd "$(dirname '$0')"
# Prepare speedbuild
mkdir out
PATH=./out:$PATH
# Build the incbin utility
gcc incbin/incbin.c -o out/incbin
# Generate resources
incbin src/scripts.rc -o src/os-scripts.cpp
# Speedbuild IceTea
g++ src/*.cpp -o out/speed.icetea
# Build ourself
speed.icetea
