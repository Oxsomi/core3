#!/bin/bash

usage() {
	echo Usage: build [Build type: Debug/Release] [Enable SIMD: On/Off]
	exit 1
}

if [ "$1" != Release ] && [ "$1" != Debug ]; then usage; fi
if [ "$2" != On ] && [ "$2" != Off ]; then usage; fi

# Build tool for host architecture + platform

echo -- Building local OxC3 CLI

currPath=`dirname "$0"`
currPath=`( cd "$currPath" && pwd )`
prevPath=$PWD

mkdir -p $currPath/builds
cd "$currPath/builds"
cmake -DCMAKE_BUILD_TYPE=$1 .. -DEnableSIMD=$2 -DForceFloatFallback=Off -DEnableTests=On -DEnableOxC3CLI=On -DEnableShaderCompiler=On ${@:3}
cmake --build . --parallel $(($(nproc) - 1)) --config $1

# Run unit test

cd "$prevPath"

mkdir -p builds/local
cp -a "$currPath/builds/bin/$1/." builds/local
echo $currPath/builds/bin/$1

mkdir -p builds/local
cd builds/local
./OxC3_test.exe
$currPath/tools/test.sh
cd ../../

# Build for remote

echo -- Building remote OxC3

mkdir -p builds/remote
cd builds/remote
cmake -DCMAKE_BUILD_TYPE=$1 ../.. -DEnableSIMD=$2 -DForceFloatFallback=Off -DEnableTests=Off -DEnableOxC3CLI=Off -DEnableShaderCompiler=Off ${@:3}
cmake --build . --parallel $(($(nproc) - 1)) --config $1
