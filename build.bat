@echo off
setlocal enabledelayedexpansion

if NOT "%1" == "Debug" (
	if NOT "%1" == "Release" (
		if NOT "%1" == "RelWithDebInfo" (
			goto usage
		)
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

if NOT "%4" == "True" (
	if NOT "%4" == "False" (
		goto usage
	)
)

for /f "tokens=4,* delims= " %%a in ("%*") do set remainder=%%b

for /f "usebackq delims=" %%A in (`conan config home`) do set conanHome=%%A

if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
	set arch=aarch64
) else (
	set arch=x64
)

set profile=packages/conan/profiles/windows_msvc_%arch%_%1

conan create packages/agility_sdk -s build_type=%1 --profile:build=%profile% --profile:host=%profile% --build=missing
conan create packages/amd_ags -s build_type=%1 --profile:build=%profile% --profile:host=%profile% --build=missing
conan create packages/nvapi -s build_type=%1 --profile:build=%profile% --profile:host=%profile% --build=missing
conan create packages/spirv_reflect -s build_type=%1 --profile:build=%profile% --profile:host=%profile% --build=missing
conan create packages/dxc -s build_type=%1 --profile:build=%profile% --profile:host=%profile% --build=missing
conan create packages/openal_soft -s build_type=%1 --profile:build=%profile% --profile:host=%profile% --build=missing
conan build . -s build_type=%1 --profile:build=%profile% --profile:host=%profile% -of build/%1/windows/%arch% -o enableSIMD=%2 -o enableTests=%3 -o dynamicLinkingGraphics=%4 !remainder!

REM Run tests

if "%3" == "False" goto :eof

cd build/%1/windows/%arch%/bin
OxC3_types_base_test.exe
OxC3_types_math_test.exe
OxC3_types_container_test.exe
OxC3_formats_oiBC_test.exe
..\..\..\..\..\tools\test.bat

cd ../../../../..
goto :eof

:usage
	echo Usage: build [Build type: Debug/Release] [Enable SIMD: True/False] [Enable Tests: True/False] [Dynamic linking: True/False]
