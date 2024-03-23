@echo off
setlocal enabledelayedexpansion

if [%2]==[] (
	goto :usage
)

if NOT "%1"=="Release" (
	if NOT "%1"=="Debug" (
		goto :usage
	)
)

if NOT "%2"=="On" (
	if NOT "%2"=="Off" (
		goto :usage
	)
)

goto :success

:usage
	echo Usage: build [Build type: Debug/Release] [Enable SIMD: On/Off]
	goto :eof

:success

rem Build normal exes

echo -- Building tests...

for /f "tokens=2,* delims= " %%a in ("%*") do set ALL_BUT_DEFINED=%%b

mkdir builds 2>nul
cd builds
cmake -DCMAKE_BUILD_TYPE=%1 .. -G "Visual Studio 17 2022" -DEnableSIMD=%2 -DForceFloatFallback=Off !ALL_BUT_DEFINED!
cmake --build . -j 8 --config %1
cd ../

rem Run unit test

builds\bin\%1\OxC3_test.exe
