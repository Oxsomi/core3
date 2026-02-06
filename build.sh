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

if [ "$(uname)" = "Darwin" ]; then
    PROFILE=packages/conan/profiles/osx_clang
else
    PROFILE=packages/conan/profiles/linux_gcc
fi

UNAME_M=$(uname -m)

case "$UNAME_M" in
    x86_64)
        ARCH=x64
        CONAN_ARCH=x86_64
        ;;
    arm64|aarch64)
        ARCH=aarch64
        CONAN_ARCH=armv8
        ;;
    *)
        echo "Unsupported architecture: $UNAME_M"
        exit 1
        ;;
esac

PROFILE="${PROFILE}_${ARCH}"

if ! conan create packages/dxc --profile:build=$PROFILE --profile:host=$PROFILE -s build_type=$1 --build=missing; then
	printf "${RED}-- Conan create DXC failed${NC}\n"
	exit 1
fi

if ! conan create packages/spirv_reflect --profile:build=$PROFILE --profile:host=$PROFILE -s build_type=$1 --build=missing; then
	printf "${RED}-- Conan create spirv_reflect failed${NC}\n"
	exit 1
fi

if ! conan create packages/nvapi --profile:build=$PROFILE --profile:host=$PROFILE -s build_type=$1 --build=missing; then
	printf "${RED}-- Conan create nvapi failed${NC}\n"
	exit 1
fi

if ! conan create packages/openal_soft --profile:build=$PROFILE --profile:host=$PROFILE -s build_type=$1 --build=missing; then
	printf "${RED}-- Conan create openal_soft failed${NC}\n"
	exit 1
fi

if [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then

	if ! conan create packages/xdg_shell --profile:build=$PROFILE --profile:host=$PROFILE -s build_type=$1 --build=missing; then
		printf "${RED}-- Conan create xdg_shell failed${NC}\n"
		exit 1
	fi
	
	if ! conan create packages/xdg_decoration --profile:build=$PROFILE --profile:host=$PROFILE -s build_type=$1 --build=missing; then
		printf "${RED}-- Conan create xdg_decoration failed${NC}\n"
		exit 1
	fi
fi

if [[ $(uname -m) == "x86_64" ]]; then
	architecture="x64"
else
	architecture="arm64"
fi
if [ "$(uname)" == "Darwin" ]; then
	platform="osx"
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
	platform="linux"
else
	platform="windows"
fi

if ! conan build . -of build/$1/$platform/$architecture --profile:build=$PROFILE --profile:host=$PROFILE -s build_type=$1 -o enableSIMD=$2 -o enableTests=$3 -o dynamicLinkingGraphics=$4 ${@:5}; then
	printf "${RED}-- Conan build failed${NC}\n"
	exit 1
fi

# Run tests

if [ "$3" == True ]; then

	cd build/$1/$platform/$architecture/bin

	if ! ./OxC3_test ; then
		printf "${RED}-- OxC3_test failed${NC}\n"
		exit 1
	fi

	if ! bash ../../../../../tools/test.sh ; then
		printf "${RED}-- test.sh failed${NC}\n"
		exit 1
	fi

	cd ../../../../..
fi
