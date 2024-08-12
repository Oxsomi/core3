#!/bin/bash

usage() {
	echo Usage: build [Build type: Debug/Release] [Enable SIMD: True/False] [Enable Tests: True/False]
	exit 1
}

if [ "$1" != Release ] && [ "$1" != Debug ]; then usage; fi
if [ "$2" != True ] && [ "$2" != False ]; then usage; fi
if [ "$3" != True ] && [ "$3" != False ]; then usage; fi

RED='\033[0;31m'
NC='\033[0m'

if ! conan create external/dxc -s build_type=$1 --build=missing; then
	printf "${RED}-- Conan create DXC failed${NC}\n"
	exit 1
fi

if ! conan build . -s build_type=$1 -o enableSIMD=$2 -o enableTests=$3 ${@:4}; then
	printf "${RED}-- Conan build failed${NC}\n"
	exit 1
fi

# Run tests

if [ "$3" == True ]; then

	cd build/$1/bin

	if ! ./OxC3_test ; then 
		printf "${RED}-- OxC3_test failed${NC}\n"
		exit 1
	fi

	if ! bash ../../../tools/test.sh ; then
		printf "${RED}-- test.sh failed${NC}\n"
		exit 1
	fi

	cd ../../..
fi
