#!/bin/bash

usage() {
	echo Usage: build [Build type: Debug/Release] [Enable SIMD: True/False] [Enable Tests: True/False]
	exit 1
}

if [ "$1" != Release ] && [ "$1" != Debug ]; then usage; fi
if [ "$2" != True ] && [ "$2" != False ]; then usage; fi
if [ "$3" != True ] && [ "$3" != False ]; then usage; fi

conan create external/dxc -s build_type=$1 --build=missing
conan build . -s build_type=$1 -o enableSIMD=$2 -o enableTests=$3 ${@:4}

# Run tests

if [ "$3" == True ]
	cd build/$1/bin
	./OxC3_test
	bash ../../../tools/test.sh
	cd ../../..
fi
