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

if [ "$(uname)" == "Darwin" ]; then
	threads=$(($(sysctl -n hw.logicalcpu) - 1))
else
	threads=$(($(nproc) - 1))
fi

cmake --build . --parallel $threads --config $1

if [ ! -d "$currPath/builds/bin/$1" ]; then
	binDir="$currPath/builds/bin"
else
	binDir="$currPath/builds/bin/$1" # Windows/MSVC uses bin/$1 (Debug, Release, etc.)
fi

# Run unit test

cd "$prevPath"

mkdir -p builds/local
cp -a "$binDir/." builds/local

if [ "$(expr substr $(uname -s) 1 10)" == "MINGW64_NT" ]; then
	executableName=./OxC3_test.exe
else 
	executableName=./OxC3_test
fi

mkdir -p builds/local
cd builds/local
echo $PWD and $binDir
$executableName
$currPath/tools/test.sh
cd ../../

# Build for remote

echo -- Building remote OxC3

mkdir -p builds/remote
cd builds/remote
cmake -DCMAKE_BUILD_TYPE=$1 ../.. -DEnableSIMD=$2 -DForceFloatFallback=Off -DEnableTests=Off -DEnableOxC3CLI=Off -DEnableShaderCompiler=Off ${@:3}
cmake --build . --parallel $threads --config $1
