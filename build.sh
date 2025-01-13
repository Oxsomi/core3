#!/bin/bash

usage() {
	echo Usage: build [Build type: Debug/Release/RelWithDebInfo] [Enable SIMD: True/False] [Enable Tests: True/False] [Dynamic linking: True/False]
	exit 1
}

if [ "$1" != Release ] && [ "$1" != Debug ] && [ "$1" != RelWithDebInfo ]; then usage; fi
if [ "$2" != True ] && [ "$2" != False ]; then usage; fi
if [ "$3" != True ] && [ "$3" != False ]; then usage; fi
if [ "$4" != True ] && [ "$4" != False ]; then usage; fi

RED='\033[0;31m'
NC='\033[0m'

conan profile detect

if ! conan create packages/dxc -s build_type=$1 --build=missing; then
	printf "${RED}-- Conan create DXC failed${NC}\n"
	exit 1
fi

if ! conan create packages/spirv_reflect -s build_type=$1 --build=missing; then
	printf "${RED}-- Conan create spirv_reflect failed${NC}\n"
	exit 1
fi

if ! conan create packages/nvapi -s build_type=$1 --build=missing; then
	printf "${RED}-- Conan create nvapi failed${NC}\n"
	exit 1
fi

if ! conan create packages/openal_soft -s build_type=$1 --build=missing; then
	printf "${RED}-- Conan create openal_soft failed${NC}\n"
	exit 1
fi

if [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then

	if ! conan create packages/xdg_shell -s build_type=$1 --build=missing; then
		printf "${RED}-- Conan create xdg_shell failed${NC}\n"
		exit 1
	fi
	
	if ! conan create packages/xdg_decoration -s build_type=$1 --build=missing; then
		printf "${RED}-- Conan create xdg_decoration failed${NC}\n"
		exit 1
	fi
fi

if ! conan build . -s build_type=$1 -o enableSIMD=$2 -o enableTests=$3 -o dynamicLinkingGraphics=$4 ${@:5}; then
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
