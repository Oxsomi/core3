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

#include "platforms/log.h"
#include "platforms/thread.h"
#include "platforms/platform.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "types/time.h"

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
	RtlCaptureStackBackTrace((DWORD)(1 + skip), (DWORD) stackSize, stack, NULL);
}

typedef struct CapturedStackTrace {

	//Module and symbol

	String mod, sym;

	//File and line don't have to be specified, for external calls

	String fil;
	U32 lin;

} CapturedStackTrace;

static const WORD COLORS[] = {
	2,	/* green */
	3,	/* cyan */
	14,	/* yellow */
	4,	/* red */
	12	/* bright red */
};

void Log_printCapturedStackTraceCustom(const void **stackTrace, U64 stackSize, ELogLevel lvl, ELogOptions opt) {

	if(!stackTrace)
		return;

	if(lvl >= ELogLevel_Count)
		return;

	if(lvl == ELogLevel_Fatal)
		lvl = ELogLevel_Error;

	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, COLORS[lvl]);

	CapturedStackTrace captured[_STACKTRACE_SIZE] = { 0 };

	U64 stackCount = 0;

	//Obtain process

	HANDLE process = GetCurrentProcess();
	HMODULE processModule = GetModuleHandleA(NULL);

	Bool hasSymbols = SymInitialize(process, NULL, TRUE);
	Bool anySymbol = false;

	if(hasSymbols)
		for (
			U64 i = 0; 
			i < stackSize && i < _STACKTRACE_SIZE && 
			stackTrace[i] && stackTrace[i] != (void*)0xCCCCCCCCCCCCCCCC; 
			++i, ++stackCount
		) {

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
			capture->mod = String_createRefAuto(modulePath, MAX_PATH);
			capture->sym = String_createRefAuto(symbolName, MAX_PATH);

			String_formatPath(&capture->sym);

			if (moduleBase == (U64)processModule)
				capture->mod = String_getFilePath(&capture->mod);

			if(line.FileName)
				capture->fil = String_createConstRefAuto(line.FileName, MAX_PATH);

			//Copy strings to heap, since they'll go out of scope

			Error err;

			if(String_length(capture->mod)) {
				String tmp = String_createNull();
				_gotoIfError(cleanup, String_createCopyx(capture->mod, &tmp));
				capture->mod = tmp;
			}

			if(String_length(capture->sym)) {
				String tmp = String_createNull();
				_gotoIfError(cleanup, String_createCopyx(capture->sym, &tmp));
				capture->sym = tmp;
			}

			if(String_length(capture->fil)) {
				String tmp = String_createNull();
				_gotoIfError(cleanup, String_createCopyx(capture->fil, &tmp));
				capture->fil = tmp;
			}

			capture->lin = line.LineNumber;
			continue;

			//Cleanup the stack if we can't move anything to heap anymore

		cleanup:
			
			for (U64 j = 0; j < i; ++j) {
				String_freex(&captured[j].fil);
				String_freex(&captured[j].sym);
				String_freex(&captured[j].mod);
			}

			Error_printx(err, lvl, opt);
			return;
		}

	if(hasSymbols && anySymbol)
		printf("Stacktrace:\n");

	else printf("Stacktrace: (No symbols)\n");

	for (U64 i = 0; i < stackCount; ++i) {

		CapturedStackTrace capture = captured[i];

		if(!String_length(capture.sym))
			printf("%p\n", stackTrace[i]);

		else if(capture.lin)
			printf(
				"%p: %.*s!%.*s (%.*s, Line %u)\n", 
				stackTrace[i], 
				(int) String_length(capture.mod), capture.mod.ptr, 
				(int) String_length(capture.sym), capture.sym.ptr,
				(int) String_length(capture.fil), capture.fil.ptr, 
				capture.lin
			);

		else printf(
			"%p: %.*s!%.*s\n", 
			stackTrace[i], 
			(int) String_length(capture.mod), capture.mod.ptr, 
			(int) String_length(capture.sym), capture.sym.ptr
		);

		//We now don't need the strings anymore

		String_freex(&capture.fil);
		String_freex(&capture.sym);
		String_freex(&capture.mod);
	}

	SymCleanup(process);
}

void Log_log(ELogLevel lvl, ELogOptions options, String arg) {

	Ns t = Time_now();

	if(lvl >= ELogLevel_Count)
		return;

	U32 thread = Thread_getId();

	//Prepare for message

	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, COLORS[lvl]);

	//[<thread> <time>]: <hr\n><ourStuff> <\n if enabled>

	Bool hasTimestamp = options & ELogOptions_Timestamp;
	Bool hasThread = options & ELogOptions_Thread;
	Bool hasNewLine = options & ELogOptions_NewLine;
	Bool hasPrepend = hasTimestamp || hasThread;

	if (hasPrepend)
		printf("[");

	if (hasThread)
		printf("%u", thread);

	if (hasTimestamp) {

		TimerFormat tf;
		Time_format(t, tf);

		printf("%s%s", hasThread ? " " : "", tf);
	}

	if (hasPrepend)
		printf("]: ");

	//Print to console and debug window

	const C8 *newLine = hasNewLine ? "\n" : "";

	printf(
		"%.*s%s", 
		(int)String_length(arg), arg.ptr,
		newLine
	);

	//Debug utils such as output to VS

	if (!IsDebuggerPresent())
		return;

	String copy = String_createNull();
	Bool panic = false;

	if(!String_isNullTerminated(arg)) {

		if (
			String_createCopyx(arg, &copy).genericError ||
			String_appendx(&copy, '\0').genericError
		) {

			OutputDebugStringA(
				"PANIC! Log_print argument was output to debugger, but wasn't null terminated\n"
				"This is normally okay, as long as a new string can be allocated.\n"
				"In this case, allocation failed, which suggests corruption or out of memory."
			);

			panic = true;
		}
	}

	else copy = String_createConstRefSized(arg.ptr, String_length(arg), String_isNullTerminated(arg));

	if(!panic)
		OutputDebugStringA(copy.ptr);

	String_freex(&copy);

	if (lvl >= ELogLevel_Error)
		DebugBreak();
}
