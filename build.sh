#!/bin/bash
# Set up CWD
cd "$(dirname '$0')"
set -ev
# Prepare speedbuild
if [ ! -d "./out" ]; then
    mkdir out
fi

# Find a suitable duo of compilers.
# C (CC)
if [ "$CC" != "" ]; then
    echo "OK"
elif which "gcc" > /dev/null; then
    CC=$(which gcc)
elif which "clang" > /dev/null; then
    CC=$(which clang)
else
    # No compiler!
    exit 1
fi
# C++ (CXX)
if [ "$CXX" != "" ]; then
    echo "OK"
elif which "g++" > /dev/null; then
    CXX=$(which g++)
elif which "clang++" > /dev/null; then
    CXX=$(which clang++)
else
    # No compiler!
    exit 1
fi


# Build the incbin utility
$CC deps/incbin/incbin.c -o out/incbin

# Generate resources
./out/incbin src/scripts.rc -o out/os-scripts.cpp

# Speedbuild IceTea
CXXFLAGS="-Wno-switch -pthread $CXXFLAGS"
$CXX -Isrc src/*.cpp out/os-scripts.cpp -o out/speed.icetea $CXXFLAGS

# Build ourself
./out/speed.icetea --it-optimize
./out/icetea -e 'print "Hello, world!"'
