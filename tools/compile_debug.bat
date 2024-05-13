@echo off
setlocal enabledelayedexpansion

rem IDxcCompiler3
rem -Zi generate pdb but don't include, output as dbg/myFile.pdb -Fd
rem -Qstrip_reflect output as dbg/myFile.ref -Fre
rem DXC_OUT_REFLECTION, DXC_OUT_PDB
rem DXC_OUT_ERRORS
rem DXC_OUT_OBJECT
rem DXC_OUT_HLSL
rem DXC_OUT_SHADER_HASH, DXC_OUT_DISASSEMBLY, DXC_OUT_REMARKS, DXC_OUT_TIME_REPORT, DXC_OUT_TIME_TRACE for debugging

rem DXC_ARG_WARNINGS_ARE_ERRORS

rem TODO: -enable-16bit-types

set extra=""
set entrypoint=""

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

if "%~2" == "rt" (
	set target=lib_6_5
	set outEntry=rt
) else (
	set outEntry=%~3
	set extra=!extra! -fspv-entrypoint-name=main
	set entrypoint=-E "%~3"
)

set extra=!extra! -fspv-target-env=vulkan1.2

echo Compiling for %target% (%~1)
echo Compiling SPIRV

mkdir "%cd%/compiled" 2> NUL
mkdir "%cd%/compiled/spirv" 2> NUL
dxc "%~1" -T "%target%" -spirv -Zpc -Od -Zi -Qembed_debug -Fo "%cd%/compiled/spirv/%~n1.%~3" -HV 2021 -I "%~dp0..\src\graphics\shaders" %extra% %entrypoint%

echo Compiling DXIL

mkdir "%cd%/compiled/dxil" 2> NUL
dxc "%~1" -T "%target%" -Zpc -Od -Zi -Qembed_debug -Fo "%cd%/compiled/dxil/%~n1.%~3" -HV 2021 -auto-binding-space 0 -E "%~3" -I "%~dp0..\src\graphics\shaders" %entrypoint%

echo Success (%~n1.!outEntry!)
