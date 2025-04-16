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
	set arch=arm64
) else (
	set arch=x64
)

set profile=%conanHome%/profiles/%1_%arch%

echo [settings] > %profile%
echo build_type=%1 >> %profile%
echo compiler=msvc >> %profile%
echo compiler.cppstd=20 >> %profile%
echo compiler.runtime=static >> %profile%
echo compiler.runtime_type=%1 >> %profile%
echo compiler.version=194 >> %profile%
echo os=Windows >> %profile%

if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
	echo arch=armv8 >> %profile%
) else (
	echo arch=x86_64 >> %profile%
)

conan create packages/agility_sdk -s build_type=%1 --profile=%profile% --build=missing
conan create packages/amd_ags -s build_type=%1 --profile=%profile% --build=missing
conan create packages/nvapi -s build_type=%1 --profile=%profile% --build=missing
conan create packages/spirv_reflect -s build_type=%1 --profile=%profile% --build=missing
conan create packages/dxc -s build_type=%1 --profile=%profile% --build=missing
conan create packages/openal_soft -s build_type=%1 --profile=%profile% --build=missing
conan build . -s build_type=%1 --profile=%profile% -of build/%1/windows/%arch% -o enableSIMD=%2 -o enableTests=%3 -o dynamicLinkingGraphics=%4 !remainder!

REM Run tests

if "%3" == "False" goto :eof

cd build/%1/windows/%arch%/bin
OxC3_test.exe
..\..\..\..\..\tools\test.bat

cd ../../../../..
goto :eof

:usage
	echo Usage: build [Build type: Debug/Release] [Enable SIMD: True/False] [Enable Tests: True/False] [Dynamic linking: True/False]
