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

//platforms/windows/wprocess.c

#include "platforms/process.h"

#ifdef SUPPORTS_PROCESS

#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/container/string_unicode.h"
#include "types/container/buffer.h"
#include "types/container/file_base.h"
#include "types/container/list_basic_types.h"
#include "types/base/error.h"
#include "types/base/time.h"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

//Captured output is capped so a runaway child can't exhaust memory.
#define PROCESS_MAX_CAPTURE (64 * MIBI)

//Append one argument to a Windows command line, quoted per the CommandLineToArgvW rules
// (double the run of backslashes that precedes a quote or the closing quote, escape embedded quotes).

static Bool Process_appendArg(CharString *cmd, CharString arg, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;
	const U64 n = CharString_length(arg);

	Bool needQuote = !n;

	for(U64 i = 0; i < n && !needQuote; ++i) {
		const C8 c = arg.ptr[i];
		if(c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '"')
			needQuote = true;
	}

	if(CharString_length(*cmd))
		gotoIfError3(clean, CharString_append(cmd, ' ', alloc, e_rr));

	if(!needQuote) {
		gotoIfError3(clean, CharString_appendString(cmd, &arg, alloc, e_rr));
		goto clean;
	}

	gotoIfError3(clean, CharString_append(cmd, '"', alloc, e_rr));

	for(U64 i = 0; i < n; ++i) {

		U64 slashes = 0;
		while(i < n && arg.ptr[i] == '\\') { ++slashes; ++i; }

		if(i == n) {                                //Trailing backslashes: double them before the closing quote

			for(U64 k = 0; k < slashes * 2; ++k)
				gotoIfError3(clean, CharString_append(cmd, '\\', alloc, e_rr));

			break;
		}

		if(arg.ptr[i] == '"') {                     //Backslashes before a quote are doubled, then the quote is escaped

			for(U64 k = 0; k < slashes * 2 + 1; ++k)
				gotoIfError3(clean, CharString_append(cmd, '\\', alloc, e_rr));

			gotoIfError3(clean, CharString_append(cmd, '"', alloc, e_rr));
		}

		else {                                      //Backslashes not before a quote stay literal

			for(U64 k = 0; k < slashes; ++k)
				gotoIfError3(clean, CharString_append(cmd, '\\', alloc, e_rr));

			gotoIfError3(clean, CharString_append(cmd, arg.ptr[i], alloc, e_rr));
		}
	}

	gotoIfError3(clean, CharString_append(cmd, '"', alloc, e_rr));

clean:
	return s_uccess;
}

//Read whatever is currently available from `pipe` into `acc`; closes the pipe (sets it to NULL) on EOF/error.
//Sets *any if it read something, so the poll loop knows to keep spinning without sleeping.

static Bool Process_drainPipe(HANDLE *pipe, ListU8 *acc, Bool *any, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if(!*pipe)
		goto clean;

	DWORD avail = 0;

	//A closed write end (child exited) turns Peek into a broken-pipe error; treat that as EOF.
	if(!PeekNamedPipe(*pipe, NULL, 0, NULL, &avail, NULL)) {
		CloseHandle(*pipe);
		*pipe = NULL;
		goto clean;
	}

	if(!avail)
		goto clean;

	U8 chunk[4096];
	DWORD toRead = avail > sizeof(chunk) ? (DWORD) sizeof(chunk) : avail;
	DWORD got = 0;

	if(!ReadFile(*pipe, chunk, toRead, &got, NULL) || !got) {
		CloseHandle(*pipe);
		*pipe = NULL;
		goto clean;
	}

	if(acc->length + got > PROCESS_MAX_CAPTURE)
		retError(clean, Error_outOfBounds(0, acc->length + got, PROCESS_MAX_CAPTURE, "Process_run() captured output too large"));

	ListU8 ref = (ListU8) { 0 };
	gotoIfError3(clean, ListU8_createRefConst(chunk, got, &ref, e_rr));
	gotoIfError3(clean, ListU8_pushAll(acc, ref, alloc, e_rr));

	*any = true;

clean:
	return s_uccess;
}

//Shared spawn core.
//`executable` is the final exe
// (a File_resolve'd absolute path, or a bare PATH name when systemSearch is set).
//systemSearch controls whether Windows resolves it through the PATH (lpApplicationName NULL).

static Bool Process_spawn(
	CharString executable,
	Bool systemSearch,
	const ListCharString *args,
	const CharString *workingDir,
	Ns maxTimeout,
	Buffer *stdOut,
	Buffer *stdErr,
	ProcessResult *result,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = Platform_instance->alloc;

	CharString cmd = CharString_createNull();
	ListU16 cmdW = (ListU16) { 0 };
	ListU16 exeW = (ListU16) { 0 };
	ListU16 workW = (ListU16) { 0 };
	ListU8 outAcc = (ListU8) { 0 };
	ListU8 errAcc = (ListU8) { 0 };

	HANDLE outRd = NULL, outWr = NULL, errRd = NULL, errWr = NULL;
	PROCESS_INFORMATION pi = { 0 };
	Bool haveProc = false;

	if(!CharString_length(executable))
		retError(clean, Error_nullPointer(0, "Process_run()::executable is required"));

	if(stdOut && stdOut->ptr)
		retError(clean, Error_invalidParameter(4, 0, "Process_run()::stdOut must be empty"));

	if(stdErr && stdErr->ptr)
		retError(clean, Error_invalidParameter(5, 0, "Process_run()::stdErr must be empty"));

	//Assemble the command line (executable is argv[0]) and convert to UTF16

	gotoIfError3(clean, Process_appendArg(&cmd, executable, alloc, e_rr));

	if(args)
		for(U64 i = 0; i < args->length; ++i)
			gotoIfError3(clean, Process_appendArg(&cmd, args->ptr[i], alloc, e_rr));

	gotoIfError3(clean, CharString_toUTF16(cmd, alloc, &cmdW, e_rr));

	//For an exact (resolved) path, hand CreateProcess the application name so it launches that file and doesn't
	// fall back to a PATH search; for a system name, leave it NULL so Windows resolves it through the PATH.

	if(!systemSearch)
		gotoIfError3(clean, CharString_toUTF16(executable, alloc, &exeW, e_rr));

	if(workingDir && CharString_length(*workingDir))
		gotoIfError3(clean, CharString_toUTF16(*workingDir, alloc, &workW, e_rr));

	//Pipes: the write end is inheritable (handed to the child), the read end is kept private to us

	SECURITY_ATTRIBUTES sa = (SECURITY_ATTRIBUTES) { .nLength = sizeof(sa), .bInheritHandle = TRUE };

	if(stdOut) {

		if(!CreatePipe(&outRd, &outWr, &sa, 0))
			retError(clean, Error_invalidState(0, "Process_run() couldn't create stdout pipe"));

		SetHandleInformation(outRd, HANDLE_FLAG_INHERIT, 0);
	}

	if(stdErr) {

		if(!CreatePipe(&errRd, &errWr, &sa, 0))
			retError(clean, Error_invalidState(0, "Process_run() couldn't create stderr pipe"));

		SetHandleInformation(errRd, HANDLE_FLAG_INHERIT, 0);
	}

	STARTUPINFOW si = (STARTUPINFOW) { .cb = sizeof(si), .dwFlags = STARTF_USESTDHANDLES };
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = stdOut ? outWr : GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError  = stdErr ? errWr : GetStdHandle(STD_ERROR_HANDLE);

	if(!CreateProcessW(
		exeW.length ? (wchar_t*) exeW.ptrNonConst : NULL, (wchar_t*) cmdW.ptrNonConst, NULL, NULL, TRUE, 0, NULL,
		workW.length ? (wchar_t*) workW.ptrNonConst : NULL, &si, &pi
	))
		retError(clean, Error_invalidState(1, "Process_spawn() CreateProcess failed (executable not found?)"));

	haveProc = true;

	//Close our copies of the write ends so the pipes hit EOF once the child (the only remaining writer) exits

	if(outWr) { CloseHandle(outWr); outWr = NULL; }
	if(errWr) { CloseHandle(errWr); errWr = NULL; }

	//Poll both pipes and the process together; reading both avoids the deadlock where the child blocks
	// filling one pipe while we block reading the other.

	const Ns start = Time_now();
	Bool timedOut = false;

	for(;;) {

		Bool any = false;
		gotoIfError3(clean, Process_drainPipe(&outRd, &outAcc, &any, alloc, e_rr));
		gotoIfError3(clean, Process_drainPipe(&errRd, &errAcc, &any, alloc, e_rr));

		const Bool exited = WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0;

		if(exited && !outRd && !errRd)
			break;

		if(!any) {

			if(maxTimeout && maxTimeout != U64_MAX && Time_now() - start > maxTimeout) {
				TerminateProcess(pi.hProcess, 1);
				timedOut = true;
			}

			Sleep(1);
		}
	}

	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	if(result)
		*result = (ProcessResult) { .exitCode = timedOut ? -1 : (I32) exitCode, .timedOut = timedOut };

	if(stdOut && outAcc.length)
		gotoIfError3(clean, Buffer_createCopy(ListU8_bufferConst(outAcc), alloc, stdOut, e_rr));

	if(stdErr && errAcc.length)
		gotoIfError3(clean, Buffer_createCopy(ListU8_bufferConst(errAcc), alloc, stdErr, e_rr));

clean:

	if(outRd) CloseHandle(outRd);
	if(outWr) CloseHandle(outWr);
	if(errRd) CloseHandle(errRd);
	if(errWr) CloseHandle(errWr);

	if(haveProc) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	ListU8_free(&outAcc, alloc);
	ListU8_free(&errAcc, alloc);
	ListU16_free(&cmdW, alloc);
	ListU16_free(&exeW, alloc);
	ListU16_free(&workW, alloc);
	CharString_free(&cmd, alloc);
	return s_uccess;
}

Bool Process_run(
	CharString executable,
	Bool isAppDir,
	const ListCharString *args,
	const CharString *workingDir,
	Ns maxTimeout,
	Buffer *stdOut,
	Buffer *stdErr,
	ProcessResult *result,
	Error *e_rr
) {

	Bool s_uccess = true;
	const Allocator *alloc = Platform_instance->alloc;
	CharString resolved = CharString_createNull();

	if(!CharString_length(executable))
		retError(clean, Error_nullPointer(0, "Process_run()::executable is required"));

	//Resolve like the rest of the file API: relative to the app or working dir, and unable to escape it

	Bool isVirtual = false;
	const CharString *base = isAppDir ? &Platform_instance->appDirectory : &Platform_instance->workDirectory;
	gotoIfError3(clean, File_resolve(&executable, &isVirtual, 0, base, alloc, &resolved, e_rr));

	if(isVirtual)
		retError(clean, Error_invalidParameter(0, 0, "Process_run()::executable can't be a virtual file"));

	gotoIfError3(clean, Process_spawn(resolved, false, args, workingDir, maxTimeout, stdOut, stdErr, result, e_rr));

clean:
	CharString_free(&resolved, alloc);
	return s_uccess;
}

Bool Process_runSystem(
	CharString executable,
	const ListCharString *args,
	const CharString *workingDir,
	Ns maxTimeout,
	Buffer *stdOut,
	Buffer *stdErr,
	ProcessResult *result,
	Error *e_rr
) {

	Bool s_uccess = true;

	if(!CharString_length(executable))
		retError(clean, Error_nullPointer(0, "Process_runSystem()::executable is required"));

	//Only a bare name, resolved via the OS PATH; reject any separator so it can't launch an arbitrary path

	for(U64 i = 0; i < CharString_length(executable); ++i)
		if(executable.ptr[i] == '/' || executable.ptr[i] == '\\')
			retError(clean, Error_invalidParameter(
				0, 0, "Process_runSystem()::executable must be a bare name (no path separators)"
			));

	gotoIfError3(clean, Process_spawn(executable, true, args, workingDir, maxTimeout, stdOut, stdErr, result, e_rr));

clean:
	return s_uccess;
}

#endif
