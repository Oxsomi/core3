@echo off
setlocal enabledelayedexpansion

if NOT "%1" == "Debug" (
	if NOT "%1" == "Release" (
		goto usage
	)	
)

if NOT "%2" == "True" (
	if NOT "%2" == "False" (
		goto usage
	)	
)

if NOT "%3" == "True" (
	if NOT "%3" == "False" (
		goto usage
	)	
)

for /f "tokens=3,* delims= " %%a in ("%*") do set remainder=%%b

conan create external/agility_sdk -s build_type=%1 --build=missing
conan create external/amd_ags -s build_type=%1 --build=missing
conan create external/nvapi -s build_type=%1 --build=missing
conan create external/spirv_reflect -s build_type=%1 --build=missing
conan create external/dxc -s build_type=%1 --build=missing
conan build . -s build_type=%1 -o enableSIMD=%2 -o enableTests=%3 !remainder!

REM Run tests

if "%3" == "False" goto :eof

cd build/bin/%1
OxC3_test.exe
..\..\..\tools\test.bat

cd ../../..
goto :eof

:usage
	echo Usage: build [Build type: Debug/Release] [Enable SIMD: True/False] [Enable Tests: True/False]
