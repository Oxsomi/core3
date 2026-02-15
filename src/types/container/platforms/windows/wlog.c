/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#include "types/container/string.h"
#include "types/container/log.h"
#include "types/base/thread.h"
#include "types/base/time.h"
#include "types/base/string_mut.h"
#include "types/container/buffer.h"
#include "types/container/list_basic_types.h"
#include "types/container/string_unicode.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/base/lock.h"
#include "types/base/constants.h"

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

//Carried over from OxC2

typedef struct CapturedStackTrace {

	//Module and symbol

	CharString mod, sym;

	//File and line don't have to be specified, for external calls

	CharString fil;
	U32 lin;

} CapturedStackTrace;

static const WORD COLORS[] = {
	2,	//green
	3,	//cyan
	14,	//yellow
	4	//red
};

SpinLock LogInitLock = { 0 };
I8 LogInitStatus = -1;

void Log_printCapturedStackTraceCustom(
	const Allocator *alloc,
	const void **stackTrace,
	U64 stackSize,
	ELogLevel lvl,
	ELogOptions opt
) {

	if(!stackTrace)
		return;

	if(lvl >= ELogLevel_Count)
		return;

	const CharString outOfStackSize = CharString_createRefCStrConst("DEV ERROR: Maximum stackSize exceeded, truncating!");

	if(stackSize > STACKTRACE_SIZE)
		Log_log(alloc, lvl, opt, &outOfStackSize);

	CapturedStackTrace captured[STACKTRACE_SIZE] = { 0 };

	U64 i = 0;

	//Init symbols if not initialized yet.

	Bool hasSymbols = false;
	HANDLE process = GetCurrentProcess();

	ELockAcquire acq = SpinLock_lock(&LogInitLock, 1 * MS);

	if (acq >= ELockAcquire_Success) {
		
		if (LogInitStatus < 0)
			LogInitStatus = (I8) SymInitialize(process, NULL, TRUE);

		hasSymbols = LogInitStatus;

		if (acq == ELockAcquire_Acquired)
			SpinLock_unlock(&LogInitLock);
	}

	CharString tmp = CharString_createNull();

	Bool anySymbol = false;

	if(hasSymbols)
		for (
			;
			i < stackSize && i < STACKTRACE_SIZE &&
			stackTrace[i] && stackTrace[i] != (void*)0xCCCCCCCCCCCCCCCC;
			++i
		) {

			const U64 addr = (U64) stackTrace[i];

			//Get module name

			const U64 moduleBase = SymGetModuleBase(process, addr);

			wchar_t modulePath[MAX_PATH + 1] = { 0 };
			if (!moduleBase || !GetModuleFileNameW((HINSTANCE)moduleBase, modulePath, MAX_PATH))
				continue;

			anySymbol = true;

			U32 symbolData[(sizeof(IMAGEHLP_SYMBOL) + MAX_PATH + 1 + 3) &~ 3] = { 0 };

			PIMAGEHLP_SYMBOL64 symbol = (PIMAGEHLP_SYMBOL64)symbolData;
			symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
			symbol->MaxNameLength = MAX_PATH;

			C8 *symbolName = symbol->Name;

			if (!SymGetSymFromAddr64(process, addr, NULL, symbol))
				continue;

			DWORD offset = 0;
			IMAGEHLP_LINE64 line = { 0 };
			line.SizeOfStruct = sizeof(line);

			SymGetLineFromAddr64(process, addr, &offset, &line);	//Can fail, meaning that line is null

			if (line.FileName && strlen(line.FileName) > MAX_PATH)
				continue;

			CapturedStackTrace *capture = captured + i;
			capture->sym = CharString_createRefAuto(symbolName, MAX_PATH);

			CharString_formatPath(&capture->sym);

			if(line.FileName)
				capture->fil = CharString_createRefAutoConst(line.FileName, MAX_PATH);

			//Copy strings to heap, since they'll go out of scope

			Bool s_uccess = true;

			if (modulePath[0])
				gotoIfError3(cleanup, CharString_createFromUTF16((const U16*) modulePath, MAX_PATH, alloc, &capture->mod, NULL));

			if(CharString_length(capture->sym)) {
				gotoIfError3(cleanup, CharString_createCopy(capture->sym, alloc, &tmp, NULL));
				capture->sym = tmp;
				tmp = CharString_createNull();
			}

			if(CharString_length(capture->fil)) {
				gotoIfError3(cleanup, CharString_createCopy(capture->fil, alloc, &tmp, NULL));
				capture->fil = tmp;
				tmp = CharString_createNull();
			}

			capture->lin = line.LineNumber;
			continue;

			//Cleanup the stack if we can't move anything to heap anymore

		cleanup:

			for (U64 j = 0; j <= i; ++j) {
				CharString_free(&captured[j].fil, alloc);
				CharString_free(&captured[j].sym, alloc);
				CharString_free(&captured[j].mod, alloc);
			}

			const CharString failedPrintStack = CharString_createRefCStrConst("Failed to print stacktrace:");
			Log_log(alloc, lvl, opt, &failedPrintStack);
			return;
		}

	opt |= ELogOptions_NoBreak | ELogOptions_NewLine;

	const CharString stackTraceMsg = CharString_createRefCStrConst("Stacktrace:");
	const CharString stackTraceNoSyms = CharString_createRefCStrConst("Stacktrace: (No symbols)");

	if(hasSymbols && anySymbol)
		Log_log(alloc, lvl, opt, &stackTraceMsg);

	else Log_log(alloc, lvl, opt, &stackTraceNoSyms);

	Bool panic = false;

	U64 stackCount = i;
	i = 0;

	for (; i < stackCount; ++i) {

		CapturedStackTrace capture = captured[i];

		if(!CharString_length(capture.sym))
			panic |= !CharString_format(alloc, &tmp, NULL, "%p\n", stackTrace[i]);

		else if(capture.lin)
			panic |= !CharString_format(alloc, &tmp, NULL,
				"%p: %.*s!%.*s (%.*s, Line %"PRIu32")\n",
				stackTrace[i],
				(int) CharString_length(capture.mod), capture.mod.ptr,
				(int) CharString_length(capture.sym), capture.sym.ptr,
				(int) CharString_length(capture.fil), capture.fil.ptr,
				capture.lin
			);

		else panic |= !CharString_format(alloc, &tmp, NULL,
			"%p: %.*s!%.*s\n",
			stackTrace[i],
			(int) CharString_length(capture.mod), capture.mod.ptr,
			(int) CharString_length(capture.sym), capture.sym.ptr
		);

		Log_log(alloc, lvl, opt &~ ELogOptions_Default, &tmp);
		CharString_free(&tmp, alloc);

		//We now don't need the strings anymore

		CharString_free(&capture.fil, alloc);
		CharString_free(&capture.sym, alloc);
		CharString_free(&capture.mod, alloc);
	}

	const CharString panicMsg = CharString_createRefCStrConst("PANIC: Failed to format one of the stacktraces");

	if(panic)
		Log_log(alloc, lvl, opt, &panicMsg);
}

static inline Bool isConsole(HANDLE stdOut) {

	if(stdOut == INVALID_HANDLE_VALUE)
		return false;
	
	U32 fileType = GetFileType(stdOut) &~ FILE_TYPE_REMOTE;

	if(fileType != FILE_TYPE_CHAR || FAILED(GetLastError()))
		return false;

	DWORD mode;
	return GetConsoleMode(stdOut, &mode);
}

SpinLock LogPrettyPrintLock;

void Log_log(const Allocator *alloc, ELogLevel lvl, ELogOptions options, const CharString *arg) {

	if(lvl >= ELogLevel_Count)
		return;

	TimeFormat tf;

	if (options & ELogOptions_Timestamp)
		Time_format(Time_now(), tf, true);

	const Bool debugger = IsDebuggerPresent();

	CharString copy = { 0 };
	ListU16 tmp = { 0 };
	ELockAcquire acq = ELockAcquire_Invalid;

	Bool panic = false;

	switch (options & (ELogOptions_Timestamp | ELogOptions_Thread | ELogOptions_NewLine)) {

		case ELogOptions_None:
		case ELogOptions_NewLine:

			if (!arg)
				break;

			panic = !CharString_createCopy(*arg, alloc, &copy, NULL);

			if((options & ELogOptions_NewLine) && !panic)
				panic |= !CharString_append(&copy, '\n', alloc, NULL);

			break;

		case ELogOptions_Thread:
		case ELogOptions_Thread | ELogOptions_NewLine:

			panic = !CharString_format(
				alloc, &copy, NULL,
				"[%"PRIu64"]: %.*s%s",
				Thread_getId(),
				!arg ? 0 : (int) CharString_length(*arg), !arg ? "" : arg->ptr,
				options & ELogOptions_NewLine ? "\n" : ""
			);

			break;

		case ELogOptions_Timestamp:
		case ELogOptions_Timestamp | ELogOptions_NewLine:

			panic = !CharString_format(
				alloc, &copy, NULL,
				"[%s]: %.*s%s",
				tf,
				!arg ? 0 : (int) CharString_length(*arg), !arg ? "" : arg->ptr,
				options & ELogOptions_NewLine ? "\n" : ""
			);

			break;

		case ELogOptions_Timestamp | ELogOptions_Thread:
		case ELogOptions_Timestamp | ELogOptions_Thread | ELogOptions_NewLine:

			panic = !CharString_format(
				alloc, &copy, NULL,
				"[%"PRIu64"%s]: %.*s%s",
				Thread_getId(), tf,
				!arg ? 0 : (int) CharString_length(*arg), !arg ? "" : arg->ptr,
				options & ELogOptions_NewLine ? "\n" : ""
			);

			break;
	}

	HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	Bool hasConsole = isConsole(stdOut);

	if(hasConsole || debugger)
		panic |= !CharString_toUTF16(copy, alloc, &tmp, NULL);

	if(CharString_length(copy) >= U32_MAX)
		goto skipConsole;

	if(debugger)
		OutputDebugStringW(tmp.ptr);

	if(!stdOut)
		goto skipConsole;

	//We have to lock to ensure that the console text is the right color.
	//Otherwise if #1 logs green, #2 logs red, it might change color in-between.

	acq = SpinLock_lock(&LogPrettyPrintLock, 1 * SECOND);

	if(acq < ELockAcquire_Success) {

		if(debugger)
			OutputDebugStringW(L"Log_print: Couldn't acquire lock. It might be stuck?\n");

		goto skipConsole;
	}

	//Remember old to ensure we can reset

	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(stdOut, &info);
	const WORD oldColor = info.wAttributes;

	//Prepare for message
	//https://archives.miloush.net/michkap/archive/2010/10/07/10072032.html
	//We can't use printf because we need colors.
	//We can't just use WriteConsole because our message might be redirected to a file!
	// This also happens when we are being executed by visual studio, git bash or on a runner or something.

	if (!panic) {

		SetConsoleTextAttribute(stdOut, COLORS[lvl]);
		
		if(hasConsole)
			WriteConsoleW(stdOut, (const wchar_t*) tmp.ptr, (int) tmp.length, NULL, NULL);

		else if(!WriteFile(stdOut, copy.ptr, (DWORD) CharString_length(copy), NULL, NULL)) {
			panic = true;
			goto skipConsole;
		}

		SetConsoleTextAttribute(stdOut, oldColor);
	}

skipConsole:

	if(panic && debugger)
		OutputDebugStringW(L"PANIC! Internal error occurred during wlog.c::Log_log, such as allocation or WriteFile");

	if(acq == ELockAcquire_Acquired)
		SpinLock_unlock(&LogPrettyPrintLock);

	CharString_free(&copy, alloc);
	ListU16_free(&tmp, alloc);

	if (debugger && lvl >= ELogLevel_Error && !(options & ELogOptions_NoBreak))
		DebugBreak();
}

CharString Error_formatPlatformError(const Allocator *alloc, const Error *e_rr) {

	if(!FAILED(e_rr->paramValue0))
		return CharString_createNull();

	wchar_t *lpBuffer = NULL;

	const DWORD f = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		(DWORD) e_rr->paramValue0,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
		(LPWSTR) &lpBuffer,
		0,
		NULL
	);

	//Unfortunately we have to copy, because we can't tell CharString to use LocalFree instead of free

	if(!f)
		return CharString_createNull();

	CharString res = CharString_createNull();
	CharString_createFromUTF16((const U16*)lpBuffer, U64_MAX, alloc, &res, NULL);

	LocalFree(lpBuffer);
	return res;
}
