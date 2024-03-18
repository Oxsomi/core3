/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

#include "types/log.h"
#include "types/thread.h"
#include "types/time.h"
#include "types/error.h"
#include "types/allocator.h"

//Unfortunately before Windows 10 it doesn't support printing colors into console using printf
//We also use Windows dependent stack tracing

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>
#include <signal.h>
#include <DbgHelp.h>
#include <stdio.h>

#pragma comment(lib, "DbgHelp.lib")

//Carried over from core2

void Log_captureStackTrace(Allocator alloc, void **stack, U64 stackSize, U8 skip) {
	(void) alloc;
	RtlCaptureStackBackTrace((DWORD)(1 + (U32)skip), (DWORD) stackSize, stack, NULL);
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
	4	/* red */
};

void Log_printCapturedStackTraceCustom(
	Allocator alloc,
	const void **stackTrace,
	U64 stackSize,
	ELogLevel lvl,
	ELogOptions opt
) {

	if(!stackTrace)
		return;

	if(lvl >= ELogLevel_Count)
		return;

	const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, COLORS[lvl]);

	CapturedStackTrace captured[STACKTRACE_SIZE] = { 0 };

	U64 stackCount = 0;

	//Obtain process

	const HANDLE process = GetCurrentProcess();

	const Bool hasSymbols = SymInitialize(process, NULL, TRUE);
	Bool anySymbol = false;

	if(hasSymbols)
		for (
			U64 i = 0;
			i < stackSize && i < STACKTRACE_SIZE &&
			stackTrace[i] && stackTrace[i] != (void*)0xCCCCCCCCCCCCCCCC;
			++i, ++stackCount
		) {

			const U64 addr = (U64) stackTrace[i];

			//Get module name

			const U64 moduleBase = SymGetModuleBase(process, addr);

			wchar_t modulePath[MAX_PATH + 1] = { 0 };
			if (!moduleBase || !GetModuleFileNameW((HINSTANCE)moduleBase, modulePath, MAX_PATH))
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
			capture->sym = CharString_createRefAuto(symbolName, MAX_PATH);

			CharString_formatPath(&capture->sym);

			if(line.FileName)
				capture->fil = CharString_createRefAutoConst(line.FileName, MAX_PATH);

			//Copy strings to heap, since they'll go out of scope

			Error err;

			if(modulePath[0])
				gotoIfError(cleanup, CharString_createFromUTF16((const U16*) modulePath, MAX_PATH, alloc, &capture->mod));

			if(CharString_length(capture->sym)) {
				CharString tmp = CharString_createNull();
				gotoIfError(cleanup, CharString_createCopy(capture->sym, alloc, &tmp));
				capture->sym = tmp;
			}

			if(CharString_length(capture->fil)) {
				CharString tmp = CharString_createNull();
				gotoIfError(cleanup, CharString_createCopy(capture->fil, alloc, &tmp));
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
				"%p: %.*s!%.*s (%.*s, Line %"PRIu32")\n",
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

	const Ns t = Time_now();

	if(lvl >= ELogLevel_Count)
		return;

	const U64 thread = Thread_getId();
	
	//Remember old to ensure we can reset

	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
	const WORD oldColor = info.wAttributes;

	//Prepare for message

	const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, COLORS[lvl]);

	//[<thread> <time>]: <hr\n><ourStuff> <\n if enabled>

	const Bool hasTimestamp = options & ELogOptions_Timestamp;
	const Bool hasThread = options & ELogOptions_Thread;
	const Bool hasNewLine = options & ELogOptions_NewLine;
	const Bool hasPrepend = hasTimestamp || hasThread;

	if (hasPrepend)
		printf("[");

	if (hasThread)
		printf("%"PRIu64, thread);

	if (hasTimestamp) {

		TimeFormat tf;
		Time_format(t, tf, true);

		printf("%s%s", hasThread ? " " : "", tf);
	}

	if (hasPrepend)
		printf("]: ");

	const Bool debugger = IsDebuggerPresent();

	ListU16 copy = (ListU16) { 0 };
	Bool panic = false;

	if (
		CharString_toUtf16(arg, alloc, &copy).genericError ||
		(hasNewLine && ListU16_insert(&copy, copy.length - 1, (U16) L'\n', alloc).genericError)
	) {

		if(debugger)
			OutputDebugStringW(
				L"PANIC! Log_print argument was output to debugger, but wasn't null terminated\n"
				L"This is normally okay, as long as a new string can be allocated.\n"
				L"In this case, allocation failed, which suggests corruption or out of memory."
			);

		panic = true;
	}

	if (!panic) {

		if(debugger)
			OutputDebugStringW((const wchar_t*)copy.ptr);

		WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), copy.ptr, (int)copy.length, NULL, NULL);
	}

	ListU16_free(&copy, alloc);
		
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), oldColor);

	if (debugger && lvl >= ELogLevel_Error)
		DebugBreak();
}
