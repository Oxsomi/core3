#include "platforms/platform.h"
#include "platforms/log.h"

#include <locale.h>
#include <signal.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>

//Handle crash signals

void sigFunc(int signal) {

	struct String msg = String_createRefUnsafeConst("Undefined instruction");

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

	Log_log(LogLevel_Fatal, LogOptions_Default, (struct LogArgs) { .argc = 1, .args = &msg });
	Log_printStackTrace(1, LogLevel_Fatal, LogOptions_Default);
	exit(signal);
}

struct Error allocCallback(void *allocator, U64 siz, struct Buffer *output) {

	allocator;

	void *ptr = malloc(siz);

	if(!output)
		return (struct Error) { .genericError = GenericError_NullPointer };

	if(!ptr)
		return (struct Error) { .genericError = GenericError_OutOfMemory };

	*output = (struct Buffer) { .ptr = ptr, .siz = siz };
	return Error_none();
}

struct Error freeCallback(void *allocator, struct Buffer buf) {
	allocator;
	free(buf.ptr);
	return Error_none();
}

int main(int argc, const char *argv[]) {
	
	#ifndef _NO_SIGNAL_HANDLING
		signal(SIGABRT, sigFunc);
		signal(SIGFPE,	sigFunc);
		signal(SIGILL,	sigFunc);
		signal(SIGINT,	sigFunc);
		signal(SIGSEGV, sigFunc);
		signal(SIGTERM, sigFunc);
	#endif

	Platform_create(argc, argv, GetModuleHandleA(NULL), freeCallback, allocCallback, NULL);

	int res = Program_run();
	Program_exit();
	Program_cleanup();

	return res;
}