/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/container/log.h"
#include "types/container/string.h"
#include "types/base/thread.h"
#include "types/base/time.h"
#include "types/container/buffer.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/base/lock.h"

//Unfortunately before Windows 10 it doesn't support printing colors into console using printf
//We also use Windows dependent stack tracing

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#define NOMINMAX
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

	CapturedStackTrace captured[STACKTRACE_SIZE] = { 0 };

	U64 stackCount = 0;

	//Obtain process

	const HANDLE process = GetCurrentProcess();
	CharString tmp = CharString_createNull();

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

			U32 symbolData[(sizeof(IMAGEHLP_SYMBOL) + MAX_PATH + 1 + 3) &~ 3] = { 0 };

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
				gotoIfError(cleanup, CharString_createFromUTF16((const U16*) modulePath, MAX_PATH, alloc, &capture->mod))

			if(CharString_length(capture->sym)) {
				gotoIfError(cleanup, CharString_createCopy(capture->sym, alloc, &tmp))
				capture->sym = tmp;
				tmp = CharString_createNull();
			}

			if(CharString_length(capture->fil)) {
				gotoIfError(cleanup, CharString_createCopy(capture->fil, alloc, &tmp))
				capture->fil = tmp;
				tmp = CharString_createNull();
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

			Log_log(alloc, lvl, opt, CharString_createRefCStrConst("Failed to print stacktrace:"));
			return;
		}

	opt |= ELogOptions_NoBreak | ELogOptions_NewLine;

	if(hasSymbols && anySymbol)
		Log_log(alloc, lvl, opt, CharString_createRefCStrConst("Stacktrace:"));

	else Log_log(alloc, lvl, opt, CharString_createRefCStrConst("Stacktrace: (No symbols)"));

	Bool panic = false;

	for (U64 i = 0; i < stackCount; ++i) {

		CapturedStackTrace capture = captured[i];

		if(!CharString_length(capture.sym))
			panic |= CharString_format(alloc, &tmp, "%p\n", stackTrace[i]).genericError;

		else if(capture.lin)
			panic |= CharString_format(alloc, &tmp,
				"%p: %.*s!%.*s (%.*s, Line %"PRIu32")\n",
				stackTrace[i],
				(int) CharString_length(capture.mod), capture.mod.ptr,
				(int) CharString_length(capture.sym), capture.sym.ptr,
				(int) CharString_length(capture.fil), capture.fil.ptr,
				capture.lin
			).genericError;

		else panic |= CharString_format(alloc, &tmp,
			"%p: %.*s!%.*s\n",
			stackTrace[i],
			(int) CharString_length(capture.mod), capture.mod.ptr,
			(int) CharString_length(capture.sym), capture.sym.ptr
		).genericError;

		Log_log(alloc, lvl, opt &~ ELogOptions_Default, tmp);
		CharString_free(&tmp, alloc);

		//We now don't need the strings anymore

		CharString_free(&capture.fil, alloc);
		CharString_free(&capture.sym, alloc);
		CharString_free(&capture.mod, alloc);
	}

	if(panic)
		Log_log(alloc, lvl, opt, CharString_createRefCStrConst("PANIC: Failed to format one of the stacktraces"));

	SymCleanup(process);
}

SpinLock lock;

void Log_log(Allocator alloc, ELogLevel lvl, ELogOptions options, CharString arg) {

	if(lvl >= ELogLevel_Count)
		return;

	const Ns t = Time_now();
	const U64 thread = Thread_getId();

	TimeFormat tf;

	if (options & ELogOptions_Timestamp)
		Time_format(t, tf, true);

	const Bool debugger = IsDebuggerPresent();

	CharString copy = (CharString) { 0 };
	ListU16 tmp = (ListU16) { 0 };

	Bool panic = false;

	switch (options & (ELogOptions_Timestamp | ELogOptions_Thread | ELogOptions_NewLine)) {

		case ELogOptions_None:
		case ELogOptions_NewLine:

			panic = CharString_createCopy(arg, alloc, &copy).genericError;

			if((options & ELogOptions_NewLine) && !panic)
				panic |= CharString_append(&copy, '\n', alloc).genericError;

			break;

		case ELogOptions_Thread:
		case ELogOptions_Thread | ELogOptions_NewLine:

			panic = CharString_format(
				alloc, &copy,
				"[%"PRIu64"]: %.*s%s",
				thread,
				(int) CharString_length(arg), arg.ptr,
				options & ELogOptions_NewLine ? "\n" : ""
			).genericError;

			break;

		case ELogOptions_Timestamp:
		case ELogOptions_Timestamp | ELogOptions_NewLine:

			panic = CharString_format(
				alloc, &copy,
				"[%s]: %.*s%s",
				tf,
				(int) CharString_length(arg), arg.ptr,
				options & ELogOptions_NewLine ? "\n" : ""
			).genericError;

			break;

		case ELogOptions_Timestamp | ELogOptions_Thread:
		case ELogOptions_Timestamp | ELogOptions_Thread| ELogOptions_NewLine:

			panic = CharString_format(
				alloc, &copy,
				"[%"PRIu64"%s]: %.*s%s",
				thread, tf,
				(int) CharString_length(arg), arg.ptr,
				options & ELogOptions_NewLine ? "\n" : ""
			).genericError;

			break;
	}

	panic |= CharString_toUTF16(copy, alloc, &tmp).genericError;

	if(debugger) {

		OutputDebugStringW(tmp.ptr);

		if(panic)
			OutputDebugStringW(
				L"PANIC! Log_print argument was output to debugger, but wasn't null terminated\n"
				L"This is normally okay, as long as a new string can be allocated.\n"
				L"In this case, allocation failed, which suggests corruption or out of memory.\n"
			);
	}

	//We have to lock to ensure that the console text is the right color.
	//Otherwise if #1 logs green, #2 logs red, it might change color in-between.

	ELockAcquire acq = SpinLock_lock(&lock, 1 * SECOND);

	if(acq < ELockAcquire_Success) {

		if(debugger)
			OutputDebugStringW(L"Log_print: Couldn't acquire lock. It might be stuck?\n");

		goto skipConsole;
	}

	//Remember old to ensure we can reset

	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
	const WORD oldColor = info.wAttributes;

	//Prepare for message

	const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, COLORS[lvl]);

	if (!panic)
		WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), (const wchar_t*) tmp.ptr, (int) tmp.length, NULL, NULL);

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), oldColor);

skipConsole:

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&lock);

	CharString_free(&copy, alloc);
	ListU16_free(&tmp, alloc);

	if (debugger && lvl >= ELogLevel_Error && !(options & ELogOptions_NoBreak))
		DebugBreak();
}

CharString Error_formatPlatformError(Allocator alloc, Error err) {

	if(err.genericError != EGenericError_PlatformError)
		return CharString_createNull();

	if(!FAILED(err.paramValue0))
		return CharString_createNull();

	wchar_t *lpBuffer = NULL;

	const DWORD f = FormatMessageW(

		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,

		NULL,
		(DWORD) err.paramValue0,

		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),

		(LPWSTR) &lpBuffer,
		0,

		NULL
	);

	//Unfortunately we have to copy, because we can't tell CharString to use LocalFree instead of free

	if(!f)
		return CharString_createNull();

	CharString res;
	if((err = CharString_createFromUTF16((const U16*)lpBuffer, U64_MAX, alloc, &res)).genericError) {
		LocalFree(lpBuffer);
		return CharString_createNull();
	}

	LocalFree(lpBuffer);
	return res;
}
