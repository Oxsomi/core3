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

//platforms/process.h

#pragma once
#include "types/base/platform_types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct CharString CharString;
typedef struct ListCharString ListCharString;

typedef struct Buffer Buffer;
typedef struct Error Error;

//Spawning child processes isn't available everywhere; sandboxed targets (web, mobile) can't exec arbitrary binaries.
//So this is gated exactly like SUPPORTS_DYNAMIC_LINKING; check SUPPORTS_PROCESS before using anything below.

#if _PLATFORM_TYPE == PLATFORM_WINDOWS || _PLATFORM_TYPE == PLATFORM_LINUX || _PLATFORM_TYPE == PLATFORM_OSX
	#define SUPPORTS_PROCESS
#endif

#ifdef SUPPORTS_PROCESS

	typedef struct ProcessResult {
		I32 exitCode;               //Process exit code (only meaningful when timedOut is false)
		Bool timedOut;              //The process was killed because it exceeded maxTimeout
		U8 pad[3];
	} ProcessResult;

	//Shared argument semantics for both spawn functions below:
	//args: argv[1..] (the executable itself is argv[0]); may be NULL/empty.
	//Each entry is one argument
	// (no shell parsing/splitting happens, so arguments with spaces stay a single argument).
	//workingDir: optional working directory for the child (NULL inherits the current one).
	//maxTimeout: 0 or U64_MAX waits forever; otherwise the child is killed after the timeout and result->timedOut is set.
	//stdOut / stdErr: optional captured output.
	//NULL leaves that stream inherited (goes to our own console).
	//When non-NULL they must be empty (Buffer_createNull()) and are filled with the raw bytes the child wrote; free them.
	//result: optional exit code / timeout status.

	//Run a LOCAL executable, resolved via File_resolve like the rest of the file API: relative to the app directory
	// (isAppDir) or the working directory, and unable to escape it.
	//Use this for bundled/app-local tools.
	impl Bool Process_run(
		CharString executable,
		Bool isAppDir,
		const ListCharString *args,
		const CharString *workingDir,
		Ns maxTimeout,
		Buffer *stdOut,
		Buffer *stdErr,
		ProcessResult *result,
		Error *e_rr
	);

	//Run a SYSTEM executable by bare name, resolved through the OS PATH.
	//`executable` must be null-terminated and
	// contain NO path separators ('/' or '\'), the same guard as DynamicLibrary_loadSystem, so it can't be abused to
	// launch an arbitrary caller-chosen path.
	impl Bool Process_runSystem(
		CharString executable,
		const ListCharString *args,
		const CharString *workingDir,
		Ns maxTimeout,
		Buffer *stdOut,
		Buffer *stdErr,
		ProcessResult *result,
		Error *e_rr
	);

#endif

#ifdef __cplusplus
	}
#endif
