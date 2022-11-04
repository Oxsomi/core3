#include "platforms/platform.h"
#include "platforms/log.h"
#include "platforms/ext/stringx.h"

#include <locale.h>
#include <signal.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

String Error_formatPlatformError(Error err) {

	if(!FAILED(err.paramValue0))
		return String_createEmpty();

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
		return String_createEmpty();

	String res;
	if((err = String_createCopyx(String_createRefConst(lpBuffer, f), &res)).genericError) {
		LocalFree(lpBuffer);
		return String_createEmpty();
	}

	//

	LocalFree(lpBuffer);
	return res;
}

//Handle crash signals

void sigFunc(int signal) {

	String msg = String_createRefUnsafeConst("Undefined instruction");

	switch (signal) {
		case SIGABRT:	msg = String_createRefUnsafeConst("Abort was called");					break;
		case SIGFPE:	msg = String_createRefUnsafeConst("Floating point error occurred");		break;
		case SIGILL:	msg = String_createRefUnsafeConst("Illegal instruction");				break;
		case SIGINT:	msg = String_createRefUnsafeConst("Interrupt was called");				break;
		case SIGSEGV:	msg = String_createRefUnsafeConst("Segfault");							break;
		case SIGTERM:	msg = String_createRefUnsafeConst("Terminate was called");				break;
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

Error allocCallback(void *allocator, U64 siz, Buffer *output) {

	allocator;

	void *ptr = malloc(siz);

	if(!output)
		return Error_nullPointer(2, 0);

	if(!ptr)
		return Error_outOfMemory(0);

	*output = (Buffer) { .ptr = ptr, .siz = siz };
	return Error_none();
}

Error freeCallback(void *allocator, Buffer buf) {
	allocator;
	free(buf.ptr);
	return Error_none();
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

	Platform_create(argc, argv, GetModuleHandleA(NULL), freeCallback, allocCallback, NULL);

	int res = Program_run();
	Program_exit();
	Program_cleanupExt();
	Program_cleanup();

	return res;
}

void Program_cleanupExt() {
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), oldColor);
}