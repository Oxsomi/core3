/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
*  
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*  
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

#include "types/buffer.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
#include "platforms/ext/listx.h"
#include "platforms/ext/archivex.h"
#include "platforms/windows/wplatform_ext.h"

#include <locale.h>
#include <signal.h>
#include <stdlib.h>

#include <intrin.h>

CharString Error_formatPlatformError(Error err) {

	if(err.genericError != EGenericError_PlatformError)
		return CharString_createNull();

	if(!FAILED(err.paramValue0))
		return CharString_createNull();

	C8 *lpBuffer = NULL;

	DWORD f = FormatMessageA(

		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,

		NULL,
		(DWORD) err.paramValue0,

		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),

		(LPTSTR)&lpBuffer,
		0,

		NULL
	);

	//Unfortunately we have to copy, because we can't tell CharString to use LocalFree instead of free

	if(!f)
		return CharString_createNull();

	CharString res;
	if((err = CharString_createCopyx(CharString_createConstRefSized(lpBuffer, f, true), &res)).genericError) {
		LocalFree(lpBuffer);
		return CharString_createNull();
	}

	//

	LocalFree(lpBuffer);
	return res;
}

//Handle crash signals

void sigFunc(int signal) {

	const C8 *msg = "Undefined instruction";

	switch (signal) {
		case SIGABRT:	msg = "Abort was called";					break;
		case SIGFPE:	msg = "Floating point error occurred";		break;
		case SIGILL:	msg = "Illegal instruction";				break;
		case SIGINT:	msg = "Interrupt was called";				break;
		case SIGSEGV:	msg = "Segfault";							break;
		case SIGTERM:	msg = "Terminate was called";				break;
	}

	//Outputting to console is not technically allowed by the Windows docs
	//If this signal is triggered from the wrong thread it might cause stackoverflow, 
	//but what are you gonna do? Crash again?
	//For debugging purposed however, this is very useful
	//Turn this off by defining _NO_SIGNAL_HANDLING

	Log_printStackTrace(1, ELogLevel_Error, ELogOptions_Default);
	Log_log(ELogLevel_Fatal, ELogOptions_Default, CharString_createConstRefCStr(msg));
	exit(signal);
}

Error allocCallback(void *allocator, U64 length, Buffer *output) {

	allocator;

	if(!output)
		return Error_nullPointer(2);

	void *ptr = malloc(length);

	if(!ptr)
		return Error_outOfMemory(0);

	*output = Buffer_createManagedPtr(ptr, length);
	return Error_none();
}

Bool freeCallback(void *allocator, Buffer buf) {
	allocator;
	free((U8*) buf.ptr);
	return true;
}

WORD oldColor = 0;

int main(int argc, const char *argv[]) {
	
	#ifndef _NO_SIGNAL_HANDLING
		signal(SIGABRT, sigFunc);
		signal(SIGFPE,	sigFunc);
		signal(SIGILL,	sigFunc);
		signal(SIGINT,	sigFunc);
		signal(SIGSEGV, sigFunc);
		signal(SIGTERM, sigFunc);
	#endif
	
	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
	oldColor = info.wAttributes;

	Error err = Platform_create(argc, argv, GetModuleHandleA(NULL), freeCallback, allocCallback, NULL);

	if(err.genericError)
		return -1;

	#if _SIMD == SIMD_SSE

		//We need to double check that our CPU supports 
		//SSE4.2, SSE4.1, (S)SSE3, SSE2, SSE, AES, PCLMULQDQ, BMI1 and RDRAND
		//https://gist.github.com/hi2p-perim/7855506
		//https://en.wikipedia.org/wiki/CPUID

		int mask3 = (1 << 25) | (1 << 26);										//SSE, SSE2

		//SSE3, PCLMULQDQ, SSSE3, SSE4.1, SSE4.2, AES, RDRAND
		int mask2 = (1 << 0) | (1 << 1) | (1 << 9) | (1 << 19) | (1 << 20) | (1 << 25) | (1 << 30);

		int cpuInfo[4];
		__cpuid(cpuInfo, 1);

		int cpuInfo1[4];
		__cpuidex(cpuInfo1, 7, 0);

		int mask1_1 = 1 << 3;				//BMI1

		//Unsupported CPU

		if ((cpuInfo[3] & mask3) != mask3 || (cpuInfo[2] & mask2) != mask2 || (cpuInfo1[1] & mask1_1) != mask1_1) {

			Log_error(
				ELogOptions_Default,
				"Unsupported CPU. The following extensions are required: "
				"SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AES, RDRAND, BMI1, PCLMULQDQ"
			);

			Platform_cleanup();
			return -1;
		}

	#endif

	int res = Program_run();
	Program_exit();
	Platform_cleanup();

	return res;
}

void Platform_cleanupExt(Platform *p) {

	//Properly clean virtual files

	for (U64 i = 0; i < Platform_instance.virtualSections.length; ++i) {
		VirtualSection *sect = ((VirtualSection*)Platform_instance.virtualSections.ptr) + i;
		CharString_freex(&sect->path);
		Archive_freex(&sect->loadedData);
	}

	List_freex(&Platform_instance.virtualSections);

	//Cleanup platform ext

	if(p->dataExt) {
		Buffer buf = Buffer_createManagedPtr(p->dataExt, sizeof(PlatformExt));
		Buffer_freex(&buf);
		p->dataExt = NULL;
	}

	//Reset console text color

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), oldColor);
}

BOOL enumerateFiles(HMODULE mod, LPCSTR unused, LPSTR name, List *sections) {

	mod; unused;

	CharString str = CharString_createConstRefCStr(name);

	Error err = Error_none();
	CharString copy = CharString_createNull();

	if(CharString_countAll(str, '/', EStringCase_Sensitive) != 1)
		Log_warnLn("Executable contained unrecognized RCDATA. Ignoring it...");

	else {

		_gotoIfError(clean, CharString_createCopyx(str, &copy));

		VirtualSection section = (VirtualSection) { .path = copy };
		_gotoIfError(clean, List_pushBackx(sections, Buffer_createConstRef(&section, sizeof(section))));
	}

clean:

	if(err.genericError) {
		sections->stride = 0;		//Signal that we failed
		CharString_freex(&copy);
	}

	return !err.genericError;
}

Error Platform_initExt(Platform *result, CharString currAppDir) {

	//

	Buffer platformExt = Buffer_createNull();
	Error err = Buffer_createEmptyBytesx(sizeof(PlatformExt), &platformExt);

	if(err.genericError)
		return err;

	PlatformExt *pext = (PlatformExt*) platformExt.ptr;

	pext->ntdll = GetModuleHandleA("ntdll.dll");

	if (!pext->ntdll) {
		Buffer_freex(&platformExt);
		return Error_platformError(0, GetLastError());
	}

	*((void**)&pext->ntDelayExecution) = (void*)GetProcAddress(pext->ntdll, "NtDelayExecution");

	if (!pext->ntDelayExecution) {
		Buffer_freex(&platformExt);
		return Error_platformError(1, GetLastError());
	}
	
	if(!Platform_useWorkingDirectory) {

		//Grab app directory of where the exe is installed

		CharString appDir = CharString_createNull();
		if ((err = CharString_createCopyx(currAppDir, &appDir)).genericError) {
			CharString_freex(&appDir);
			Buffer_freex(&platformExt);
			return Error_platformError(1, GetLastError());
		}

		CharString_replaceAll(&appDir, '\\', '/', EStringCase_Sensitive);

		U64 loc = CharString_findLast(appDir, '/', EStringCase_Sensitive);
		CharString basePath = CharString_createNull();

		if (loc == CharString_length(appDir))
			basePath = CharString_createConstRefAuto(appDir.ptr, CharString_length(appDir));
	
		else CharString_cut(appDir, 0, loc + 1, &basePath);

		CharString workDir = CharString_createNull();

		if ((err = CharString_createCopyx(basePath, &workDir)).genericError) {
			CharString_freex(&appDir);
			Buffer_freex(&platformExt);
			return Error_platformError(1, GetLastError());
		}

		if(
			!CharString_endsWith(basePath, '/', EStringCase_Sensitive) && 
			(err = CharString_appendx(&workDir, '/')).genericError
		)  {
			CharString_freex(&appDir);
			CharString_freex(&workDir);
			Buffer_freex(&platformExt);
			return err;
		}

		CharString_freex(&appDir);
		result->workingDirectory = workDir;

	} else {

		//Init working dir

		C8 buff[MAX_PATH + 1];
		DWORD chars = GetCurrentDirectoryA(MAX_PATH + 1, buff);

		if(!chars) {
			Buffer_freex(&platformExt);
			return Error_platformError(0, GetLastError());		//Needs no cleaning. cleanup(Ext) will handle it
		}

		//Move to heap and standardize

		if((err = CharString_createCopyx(CharString_createConstRefSized(buff, chars, true), &result->workingDirectory)).genericError) {
			Buffer_freex(&platformExt);
			return err;
		}

		CharString_replaceAll(&result->workingDirectory, '\\', '/', EStringCase_Sensitive);

		if ((err = CharString_appendx(&result->workingDirectory, '/')).genericError)  {
			CharString_freex(&result->workingDirectory);
			Buffer_freex(&platformExt);
			return err;
		}
	}
	
	//Init virtual files

	result->virtualSections = List_createEmpty(sizeof(VirtualSection));
	if (!EnumResourceNamesA(
		NULL, RT_RCDATA,
		(ENUMRESNAMEPROCA)enumerateFiles,
		(LONG_PTR)&result->virtualSections
	)) {

		//Enum resource names also fails if we don't have any resources.
		//To counter this, enumerateFiles sets stride to 0 if the reason it returned false was because of the function.

		if(!result->virtualSections.stride) {

			result->virtualSections.stride = sizeof(VirtualSection);

			List_freex(&result->virtualSections);
			CharString_freex(&result->workingDirectory);
			Buffer_freex(&platformExt);
			return Error_invalidState(1);
		}
	}

	//

	result->dataExt = pext;
	return Error_none();
}
