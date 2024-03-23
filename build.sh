#!/bin/sh

usage() {
	echo Usage: build [Build type: Debug/Release] [Enable SIMD: On/Off]
	exit 1
}

if [ "$1" != Release ] && [ "$1" != Debug ]; then usage; fi
if [ "$2" != On ] && [ "$2" != Off ]; then usage; fi

# Build normal exes

echo -- Building tests...

mkdir -p builds
cd builds
cmake -DCMAKE_BUILD_TYPE=$1 .. -DEnableSIMD=$2 -DForceFloatFallback=Off ${@:3}
cmake --build . -j 8 --config $1
cd ../

# Run unit test

./builds/bin/OxC3_test
