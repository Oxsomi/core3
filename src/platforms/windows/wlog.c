#include "platforms/log.h"
#include "platforms/thread.h"
#include "platforms/platform.h"
#include "platforms/errorx.h"
#include "types/timer.h"

//Unfortunately before Windows 10 it doesn't support printing colors into console using printf
//We also use Windows dependent stack tracing

#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <signal.h>
#include <DbgHelp.h>
#include <stdio.h>

#pragma comment(lib, "DbgHelp.lib")

//Carried over from core2

void Log_captureStackTrace(void **stack, U64 stackSize, U64 skip) {
	RtlCaptureStackBackTrace(
		(DWORD)(1 + skip), 
		(DWORD) stackSize, 
		stack, NULL
	);
}

typedef struct CapturedStackTrace {

	//Module and symbol

	String mod, sym;

	//File and line don't have to be specified, for external calls

	String fil;
	U32 lin;
} CapturedStackTrace;

static const WORD colors[] = {
	2,	/* green */
	3,	/* cyan */
	14,	/* yellow */
	4,	/* red */
	12	/* bright red */
};

void Log_printCapturedStackTraceCustom(const void **stackTrace, U64 stackSize, ELogLevel lvl, ELogOptions opt) {

	if(lvl == ELogLevel_Fatal)
		lvl = ELogLevel_Error;

	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, colors[lvl]);

	CapturedStackTrace captured[StackTrace_SIZE] = { 0 };

	U64 stackCount = 0;

	//Obtain process

	HANDLE process = GetCurrentProcess();
	HMODULE processModule = GetModuleHandleA(NULL);

	Bool hasSymbols = SymInitialize(process, NULL, TRUE);
	Bool anySymbol = false;

	if(hasSymbols)
		for (U64 i = 0; i < stackSize && i < StackTrace_SIZE && stackTrace[i]; ++i, ++stackCount) {

			U64 addr = (U64) stackTrace[i];

			//Get module name

			U64 moduleBase = SymGetModuleBase(process, addr);

			C8 modulePath[MAX_PATH + 1] = { 0 };
			if (!moduleBase || !GetModuleFileNameA((HINSTANCE)moduleBase, modulePath, MAX_PATH))
				continue;

			anySymbol = true;

			C8 symbolData[sizeof(IMAGEHLP_SYMBOL) + MAX_PATH + 1] = { 0 };

			PIMAGEHLP_SYMBOL symbol = (PIMAGEHLP_SYMBOL)symbolData;
			symbol->SizeOfStruct = sizeof(symbolData);
			symbol->MaxNameLength = MAX_PATH;

			C8 *symbolName = symbol->Name;

			if (!SymGetSymFromAddr(process, addr, NULL, symbol))
				continue;

			DWORD offset = 0;
			IMAGEHLP_LINE line = { 0 };
			line.SizeOfStruct = sizeof(line);

			SymGetLineFromAddr(process, addr, &offset, &line);	//Can fail, meaning that line is null

			if (line.FileName && strlen(line.FileName) > MAX_PATH)
				continue;

			CapturedStackTrace *capture = captured + i;
			capture->mod = String_createRefDynamic(modulePath, MAX_PATH);
			capture->sym = String_createRefDynamic(symbolName, MAX_PATH);

			String_formatPath(&capture->sym);

			if (moduleBase == (U64)processModule)
				capture->mod = String_getFilePath(&capture->mod);

			if(line.FileName)
				capture->fil = String_createRefConst(line.FileName, MAX_PATH);

			//Copy strings to heap, since they'll go out of scope

			Error err;

			if(capture->mod.len)
				if ((err = String_createCopy(capture->mod, EPlatform_instance.alloc, &capture->mod)).genericError)
					goto cleanup;

			if(capture->sym.len)
				if ((err = String_createCopy(capture->sym, EPlatform_instance.alloc, &capture->sym)).genericError)
					goto cleanup;

			if(capture->fil.len)
				if ((err = String_createCopy(capture->fil, EPlatform_instance.alloc, &capture->fil)).genericError)
					goto cleanup;

			capture->lin = line.LineNumber;
			continue;

			//Cleanup the stack if we can't move anything to heap anymore

		cleanup:
			
			for (U64 j = 0; j < i; ++j) {
				String_free(&captured[j].fil, EPlatform_instance.alloc);
				String_free(&captured[j].sym, EPlatform_instance.alloc);
				String_free(&captured[j].mod, EPlatform_instance.alloc);
			}

			Error_printx(err, lvl, opt);
			return;
		}

	if(hasSymbols && anySymbol)
		printf("Stacktrace:\n");

	else printf("Stacktrace: (No symbols)\n");

	for (U64 i = 0; i < stackCount; ++i) {

		CapturedStackTrace capture = captured[i];

		if(!capture.sym.len)
			printf("%p\n", stackTrace[i]);

		else if(capture.lin)
			printf(
				"%p: %.*s!%.*s (%.*s, Line %u)\n", 
				stackTrace[i], 
				(int) U64_min(I32_MAX, capture.mod.len), capture.mod.ptr, 
				(int) U64_min(I32_MAX, capture.sym.len), capture.sym.ptr,
				(int) U64_min(I32_MAX, capture.fil.len), capture.fil.ptr, 
				capture.lin
			);

		else printf(
			"%p: %.*s!%.*s\n", 
			stackTrace[i], 
			(int) U64_min(I32_MAX, capture.mod.len), capture.mod.ptr, 
			(int) U64_min(I32_MAX, capture.sym.len), capture.sym.ptr
		);

		//We now don't need the strings anymore

		String_free(&capture.fil, EPlatform_instance.alloc);
		String_free(&capture.sym, EPlatform_instance.alloc);
		String_free(&capture.mod, EPlatform_instance.alloc);
	}

	if(hasSymbols)
		SymCleanup(process);
}

void Log_log(ELogLevel lvl, ELogOptions options, LogArgs args) {


	Ns t = Timer_now();

	U32 thread = Thread_getId();

	const C8 *hrErr = "";

	if (lvl > ELogLevel_Debug) {

		HRESULT hr = GetLastError();
		
		if (!SUCCEEDED(hr))
			FormatMessageA(

				FORMAT_MESSAGE_FROM_SYSTEM | 
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_IGNORE_INSERTS,  

				NULL,
				hr,
				MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL),

				(LPTSTR) &hrErr,  

				0, NULL
			);
	}

	//Prepare for message

	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, colors[lvl]);

	//[<thread> <time>]: <hr\n><ourStuff> <\n if enabled>

	Bool hasTimestamp = options & ELogOptions_Timestamp;
	Bool hasThread = options & ELogOptions_Thread;
	Bool hasNewLine = options & ELogOptions_NewLine;
	Bool hasPrepend = hasTimestamp || hasThread;

	if (hasPrepend)
		printf("[");

	if (hasThread) {

		LongString str;
		Log_num10(str, thread);

		printf("%s", str);
	}

	if (hasTimestamp) {

		TimerFormat tf;
		Timer_format(t, tf);

		printf("%s%s", hasThread ? " " : "", tf);
	}

	if (hasPrepend)
		printf("]: ");

	if (strlen(hrErr))
		printf("%s\n", hrErr);

	//Print to console and debug window

	const C8 *newLine = hasNewLine ? "\n" : "";

	for (U64 i = 0; i < args.argc; ++i)
		printf(
			"%.*s%s", 
			(int) U64_min(I32_MAX, args.args[i].len), args.args[i].ptr,
			newLine
		);

	//Debug utils such as output to VS

	if (!IsDebuggerPresent())
		return;

	for (U64 i = 0; i < args.argc; ++i) {

		OutputDebugStringA(args.args[i].ptr);

		if (hasNewLine)
			OutputDebugStringA("\n");
	}

	if (lvl >= ELogLevel_Error)
		DebugBreak();
}
