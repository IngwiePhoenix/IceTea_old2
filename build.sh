#!/bin/bash
# Set up CWD
cd "$(dirname '$0')"
set -ev
# Prepare speedbuild
if [ ! -d "./out" ]; then
    mkdir out
fi

# Build the incbin utility
gcc deps/incbin/incbin.c -o out/incbin

# Generate resources
./out/incbin src/scripts.rc -o out/os-scripts.cpp

# Speedbuild IceTea
CXXFLAGS="-Wno-switch -pthread $CXXFLAGS"
g++ -Isrc src/*.cpp out/os-scripts.cpp -o out/speed.icetea $CXXFLAGS

# Build ourself
./out/speed.icetea --it-optimize
