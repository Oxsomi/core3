#include "types/buffer.h"
#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/ext/stringx.h"
#include "platforms/ext/bufferx.h"
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
	if((err = String_createCopyx(String_createConstRef(lpBuffer, f), &res)).genericError) {
		LocalFree(lpBuffer);
		return String_createNull();
	}

	//

	LocalFree(lpBuffer);
	return res;
}

//Handle crash signals

void sigFunc(int signal) {

	String msg = String_createConstRefUnsafe("Undefined instruction");

	switch (signal) {
		case SIGABRT:	msg = String_createConstRefUnsafe("Abort was called");					break;
		case SIGFPE:	msg = String_createConstRefUnsafe("Floating point error occurred");		break;
		case SIGILL:	msg = String_createConstRefUnsafe("Illegal instruction");				break;
		case SIGINT:	msg = String_createConstRefUnsafe("Interrupt was called");				break;
		case SIGSEGV:	msg = String_createConstRefUnsafe("Segfault");							break;
		case SIGTERM:	msg = String_createConstRefUnsafe("Terminate was called");				break;
	}

	//Outputting to console is not technically allowed by the Windows docs
	//If this signal is triggered from the wrong thread it might cause stackoverflow, 
	//but what are you gonna do? Crash again?
	//For debugging purposed however, this is very useful
	//Turn this off by defining _NO_SIGNAL_HANDLING

	Log_log(ELogLevel_Fatal, ELogOptions_Default, (LogArgs) { .argc = 1, .args = &msg });
	Log_printStackTrace(1, ELogLevel_Fatal, ELogOptions_Default);
	exit(signal);
}

Error allocCallback(void *allocator, U64 length, Buffer *output) {

	allocator;

	if(!output)
		return Error_nullPointer(2, 0);

	void *ptr = malloc(length);

	if(!ptr)
		return Error_outOfMemory(0);

	*output = Buffer_createManagedPtr(ptr, length);
	return Error_none();
}

Bool freeCallback(void *allocator, Buffer buf) {
	allocator;
	free(buf.ptr);
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
				String_createConstRefUnsafe(
					"Unsupported CPU. The following extensions are required: "
					"SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AES"
				),
				ELogOptions_Default
			);

			Platform_cleanup();
			return -1;
		}

	#endif

	int res = Program_run();
	Program_exit();
	Platform_cleanupExt(&Platform_instance);
	Platform_cleanup();

	return res;
}

void Platform_cleanupExt(Platform *p) {

	//Cleanup platform ext

	if(p->dataExt) {
		Buffer buf = Buffer_createManagedPtr(p->dataExt, sizeof(PlatformExt));
		Buffer_freex(&buf);
		p->dataExt = NULL;
	}

	//Reset console text color

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), oldColor);
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
	
	#if !_WORKING_DIR

		//Grab app directory of where the exe is installed

		String appDir;
		if ((err = String_createCopyx(currAppDir, &appDir)).genericError)
			goto cleanup;

		String_replaceAll(&appDir, '\\', '/', EStringCase_Sensitive);

		U64 loc = String_findLast(appDir, '/', EStringCase_Sensitive);
		String basePath;

		if (loc == appDir.length)
			basePath = String_createConstRef(appDir.ptr, appDir.length);
	
		else String_cut(appDir, 0, loc + 1, &basePath);

		String workDir;

		if ((err = String_createCopyx(basePath, &workDir)).genericError)
			goto cleanup;

		String_freex(&appDir);
		result->workingDirectory = workDir;

	#else

		currAppDir;

		//Init working dir

		C8 buff[MAX_PATH + 1];
		DWORD chars = GetCurrentDirectoryA(MAX_PATH + 1, buff);

		if(!chars) {
			Buffer_freex(&platformExt);
			return Error_platformError(0, GetLastError());		//Needs no cleaning. cleanup(Ext) will handle it
		}

		//Move to heap and standardize

		if((err = String_createCopyx(String_createConstRef(buff, chars), &result->workingDirectory)).genericError) {
			Buffer_freex(&platformExt);
			return err;
		}

		String_replaceAll(&result->workingDirectory, '\\', '/', EStringCase_Sensitive);

		if ((err = String_appendx(&result->workingDirectory, '/')).genericError)  {
			String_freex(&result->workingDirectory);
			Buffer_freex(&platformExt);
			return err;
		}

	#endif

	result->dataExt = pext;
	return Error_none();
}
