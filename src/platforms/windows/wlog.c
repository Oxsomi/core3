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

	CharString mod, sym;

	//File and line don't have to be specified, for external calls

	CharString fil;
	U32 lin;

} CapturedStackTrace;

static const WORD COLORS[] = {
	2,	/* green */
	3,	/* cyan */
	14,	/* yellow */
	4,	/* red */
	12	/* bright red */
};

void Log_printCapturedStackTraceCustom(Allocator alloc, const void **stackTrace, U64 stackSize, ELogLevel lvl, ELogOptions opt) {

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
			capture->mod = CharString_createRefAuto(modulePath, MAX_PATH);
			capture->sym = CharString_createRefAuto(symbolName, MAX_PATH);

			CharString_formatPath(&capture->sym);

			if (moduleBase == (U64)processModule)
				capture->mod = CharString_getFilePath(&capture->mod);

			if(line.FileName)
				capture->fil = CharString_createConstRefAuto(line.FileName, MAX_PATH);

			//Copy strings to heap, since they'll go out of scope

			Error err;

			if(CharString_length(capture->mod)) {
				CharString tmp = CharString_createNull();
				_gotoIfError(cleanup, CharString_createCopy(capture->mod, alloc, &tmp));
				capture->mod = tmp;
			}

			if(CharString_length(capture->sym)) {
				CharString tmp = CharString_createNull();
				_gotoIfError(cleanup, CharString_createCopy(capture->sym, alloc, &tmp));
				capture->sym = tmp;
			}

			if(CharString_length(capture->fil)) {
				CharString tmp = CharString_createNull();
				_gotoIfError(cleanup, CharString_createCopy(capture->fil, alloc, &tmp));
				capture->fil = tmp;
			}

			capture->lin = line.LineNumber;
			continue;

			//Cleanup the stack if we can't move anything to heap anymore

		cleanup:
			
			for (U64 j = 0; j < i; ++j) {
				CharString_free(&captured[j].fil, alloc);
				CharString_free(&captured[j].sym, alloc);
				CharString_free(&captured[j].mod, alloc);
			}

			Error_print(alloc, err, lvl, opt);
			return;
		}

	if(hasSymbols && anySymbol)
		printf("Stacktrace:\n");

	else printf("Stacktrace: (No symbols)\n");

	for (U64 i = 0; i < stackCount; ++i) {

		CapturedStackTrace capture = captured[i];

		if(!CharString_length(capture.sym))
			printf("%p\n", stackTrace[i]);

		else if(capture.lin)
			printf(
				"%p: %.*s!%.*s (%.*s, Line %u)\n", 
				stackTrace[i], 
				(int) CharString_length(capture.mod), capture.mod.ptr, 
				(int) CharString_length(capture.sym), capture.sym.ptr,
				(int) CharString_length(capture.fil), capture.fil.ptr, 
				capture.lin
			);

		else printf(
			"%p: %.*s!%.*s\n", 
			stackTrace[i], 
			(int) CharString_length(capture.mod), capture.mod.ptr, 
			(int) CharString_length(capture.sym), capture.sym.ptr
		);

		//We now don't need the strings anymore

		CharString_free(&capture.fil, alloc);
		CharString_free(&capture.sym, alloc);
		CharString_free(&capture.mod, alloc);
	}

	SymCleanup(process);
}

void Log_log(Allocator alloc, ELogLevel lvl, ELogOptions options, CharString arg) {

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

		TimeFormat tf;
		Time_format(t, tf, true);

		printf("%s%s", hasThread ? " " : "", tf);
	}

	if (hasPrepend)
		printf("]: ");

	//Print to console and debug window

	const C8 *newLine = hasNewLine ? "\n" : "";

	printf(
		"%.*s%s", 
		(int)CharString_length(arg), arg.ptr,
		newLine
	);

	//Debug utils such as output to VS

	if (!IsDebuggerPresent())
		return;

	CharString copy = CharString_createNull();
	Bool panic = false;

	if (
		CharString_createCopy(arg, alloc, &copy).genericError ||
		(hasNewLine && CharString_append(&copy, '\n', alloc).genericError)
	) {

		OutputDebugStringA(
			"PANIC! Log_print argument was output to debugger, but wasn't null terminated\n"
			"This is normally okay, as long as a new string can be allocated.\n"
			"In this case, allocation failed, which suggests corruption or out of memory."
		);

		panic = true;
	}

	if(!panic)
		OutputDebugStringA(copy.ptr);

	CharString_free(&copy, alloc);

	if (lvl >= ELogLevel_Error)
		DebugBreak();
}
