@echo off
setlocal enabledelayedexpansion

rem TODO: -enable-16bit-types

set extra=""

if "%~2" == "cs" set target=cs_6_5
if "%~2" == "ps" (
	set target=ps_6_5
	set extra=-fvk-use-dx-position-w
)

if "%~2" == "hs" set target=hs_6_5

if "%~2" == "vs" (
	set target=vs_6_5
	set extra=-fvk-invert-y
)

if "%~2" == "gs" (
	set target=gs_6_5
	set extra=-fvk-invert-y
)

if "%~2" == "ds" (
	set target=ds_6_5
	set extra=-fvk-invert-y
)

echo Compiling for %target% (%~1)
echo Compiling SPIRV

mkdir "%~dp1spirv" 2> NUL
dxc "%~1" -T "%target%" -spirv -Zpc -Od -Zi -Qembed_debug -Fo "%~dp1spirv/%~n1.%~3" -HV 2021 -E "%~3" -I "%~dp0..\src\graphics\shaders" -fspv-entrypoint-name=main %extra%

echo Compiling DXIL

mkdir "%~dp1dxil" 2> NUL
dxc "%~1" -T "%target%" -Zpc -Od -Zi -Qembed_debug -Fo "%~dp1dxil/%~n1.%~3" -HV 2021 -E "%~3" -I "%~dp0..\src\graphics\shaders"

echo Success (%~n1.%~3)