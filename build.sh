#!/bin/sh

usage() {
	echo Usage: build [Build type: Debug/Release] [Enable SIMD: On/Off] [Force float fallback: On/Off]
	exit 1
}

if [ "$1" != Release ] && [ "$1" != Debug ]; then usage; fi
if [ "$2" != On ] && [ "$2" != Off ]; then usage; fi
if [ "$3" != On ] && [ "$3" != Off ]; then usage; fi

# Build normal exes

echo -- Building tests...

mkdir builds
cd builds
cmake -DCMAKE_BUILD_TYPE=$1 .. -DEnableSIMD=$2 -DForceFloatFallback=$3
cmake --build . -j 8 --config $1
cd ../

rem Run unit test

./builds/bin/$1/OxC3_test.exe
