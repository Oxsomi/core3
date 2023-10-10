@echo off

rem TODO: -enable-16bit-types
rem TODO: debug: -Od -Zi -Qembed_debug and remove -Qstrip_debug

mkdir "%~dp1spirv" 2> NUL
dxc "%~1" -T "%~2" -spirv -Fo "%~dp1spirv/%~n1.%~3" -HV 2021 -Qstrip_debug -E "%~3"

echo Compiled SPIR-V

mkdir "%~dp1dxil" 2> NUL
dxc "%~1" -T "%~2" -Fo "%~dp1dxil/%~n1.%~3" -HV 2021 -Qstrip_debug -E "%~3"

echo Compiled DXIL