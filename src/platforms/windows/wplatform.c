/* MIT License
*   
*  Copyright (c) 2022 Oxsomi, Nielsbishere (Niels Brunekreef)
*  
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*  
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.
*  
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE. 
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

String Error_formatPlatformError(Error err) {

	if(!FAILED(err.paramValue0))
		return String_createNull();

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

	//Unfortunately we have to copy, because we can't tell String to use LocalFree instead of free

	if(!f)
		return String_createNull();

	String res;
	if((err = String_createCopyx(String_createConstRefSized(lpBuffer, f, true), &res)).genericError) {
		LocalFree(lpBuffer);
		return String_createNull();
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

	Log_log(ELogLevel_Fatal, ELogOptions_Default, String_createConstRefUnsafe(msg));
	Log_printStackTrace(1, ELogLevel_Fatal, ELogOptions_Default);
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
		//AVX, SSE4.2, SSE4.1, (S)SSE3, SSE2, SSE, AES, PCLMULQDQ and RDRAND
		//https://gist.github.com/hi2p-perim/7855506
		//https://en.wikipedia.org/wiki/CPUID

		int mask3 = (1 << 25) | (1 << 26);										//SSE, SSE2

		//SSE3, PCLMULQDQ, SSSE3, SSE4.1, SSE4.2, AES, RDRAND
		int mask2 = (1 << 0) | (1 << 1) | (1 << 9) | (1 << 19) | (1 << 20) | (1 << 25) | (1 << 30);

		int cpuInfo[4];
		__cpuid(cpuInfo, 1);

		//Unsupported CPU

		if ((cpuInfo[3] & mask3) != mask3 || (cpuInfo[2] & mask2) != mask2) {

			Log_error(
				ELogOptions_Default,
				"Unsupported CPU. The following extensions are required: "
				"SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AES"
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
		String_freex(&sect->path);
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

	String str = String_createConstRefUnsafe(name);

	Error err = Error_none();
	String copy = String_createNull();

	if(String_countAll(str, '/', EStringCase_Sensitive) != 1)
		Log_warnLn("Executable contained unrecognized RCDATA. Ignoring it...");

	else {

		_gotoIfError(clean, String_createCopyx(str, &copy));

		VirtualSection section = (VirtualSection) { .path = copy };
		_gotoIfError(clean, List_pushBackx(sections, Buffer_createConstRef(&section, sizeof(section))));
	}

clean:

	if(err.genericError) {
		sections->stride = 0;		//Signal that we failed
		String_freex(&copy);
	}

	return !err.genericError;
}

Error Platform_initExt(Platform *result, String currAppDir) {

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

		String appDir = String_createNull();
		if ((err = String_createCopyx(currAppDir, &appDir)).genericError) {
			String_freex(&appDir);
			Buffer_freex(&platformExt);
			return Error_platformError(1, GetLastError());
		}

		String_replaceAll(&appDir, '\\', '/', EStringCase_Sensitive);

		U64 loc = String_findLast(appDir, '/', EStringCase_Sensitive);
		String basePath = String_createNull();

		if (loc == String_length(appDir))
			basePath = String_createConstRefAuto(appDir.ptr, String_length(appDir));
	
		else String_cut(appDir, 0, loc + 1, &basePath);

		String workDir = String_createNull();

		if ((err = String_createCopyx(basePath, &workDir)).genericError) {
			String_freex(&appDir);
			Buffer_freex(&platformExt);
			return Error_platformError(1, GetLastError());
		}

		if ((err = String_appendx(&workDir, '/')).genericError)  {
			String_freex(&appDir);
			String_freex(&workDir);
			Buffer_freex(&platformExt);
			return err;
		}

		String_freex(&appDir);
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

		if((err = String_createCopyx(String_createConstRefSized(buff, chars, true), &result->workingDirectory)).genericError) {
			Buffer_freex(&platformExt);
			return err;
		}

		String_replaceAll(&result->workingDirectory, '\\', '/', EStringCase_Sensitive);

		if ((err = String_appendx(&result->workingDirectory, '/')).genericError)  {
			String_freex(&result->workingDirectory);
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
			String_freex(&result->workingDirectory);
			Buffer_freex(&platformExt);
			return Error_invalidState(0);
		}
	}

	//

	result->dataExt = pext;
	return Error_none();
}
