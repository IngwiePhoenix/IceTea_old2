#!/bin/bash
# Set up CWD
cd "$(dirname '$0')"
set -v
# Prepare speedbuild
if [ ! -d "./out" ]; then
    mkdir out
fi

# Build the incbin utility
gcc incbin/incbin.c -o out/incbin

# Generate resources
./out/incbin src/scripts.rc -o src/os-scripts.cpp

# Speedbuild IceTea
CXXFLAGS="-Wno-switch $CXXFLAGS"
g++ src/*.cpp -o out/speed.icetea $CXXFLAGS

# Build ourself
./out/speed.icetea
