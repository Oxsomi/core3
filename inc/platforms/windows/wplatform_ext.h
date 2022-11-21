#pragma once
#include "types/types.h"

#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

typedef struct PlatformExt {

	HMODULE ntdll;
	void (*ntDelayExecution)(Bool, PLARGE_INTEGER);

} PlatformExt;
