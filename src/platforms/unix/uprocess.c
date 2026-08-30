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

//platforms/unix/uprocess.c

#include "platforms/process.h"

#ifdef SUPPORTS_PROCESS

#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/container/buffer.h"
#include "types/container/file_base.h"
#include "types/container/list_basic_types.h"
#include "types/base/error.h"
#include "types/base/time.h"

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <stdlib.h>

//Captured output is capped so a runaway child can't exhaust memory.
#define PROCESS_MAX_CAPTURE (64 * MIBI)

//A null-terminated copy of a CharString (execvp/chdir need C strings, and CharString isn't guaranteed terminated).
//Returns NULL on allocation failure.
//malloc is used deliberately: this array outlives the OxC3 allocator across fork.

static char *Process_dupCStr(CharString s) {

	const U64 n = CharString_length(s);
	char *c = (char*) malloc(n + 1);

	if(!c)
		return NULL;

	if(n)
		Buffer_memcpy(Buffer_createRef(c, n), Buffer_createRefConst(s.ptr, n));

	c[n] = '\0';
	return c;
}

//Shared spawn core.
//`executable` is the final exe: a File_resolve'd absolute path
// (execvp uses it as-is because it contains '/') or a bare PATH name (execvp searches the PATH).
//So the two public wrappers differ only in resolution.

static Bool Process_spawn(
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
	const Allocator *alloc = Platform_instance->alloc;

	ListU8 outAcc = (ListU8) { 0 };
	ListU8 errAcc = (ListU8) { 0 };

	int outFd[2] = { -1, -1 };
	int errFd[2] = { -1, -1 };
	pid_t pid = -1;

	char **argv = NULL;
	char *workDir = NULL;
	U64 argCount = 0;

	if(!CharString_length(executable))
		retError(clean, Error_nullPointer(0, "Process_run()::executable is required"));

	if(stdOut && stdOut->ptr)
		retError(clean, Error_invalidParameter(4, 0, "Process_run()::stdOut must be empty"));

	if(stdErr && stdErr->ptr)
		retError(clean, Error_invalidParameter(5, 0, "Process_run()::stdErr must be empty"));

	//Build a null-terminated argv (executable + args + NULL) before forking; the child mustn't allocate

	argCount = 1 + (args ? args->length : 0);
	argv = (char**) calloc(argCount + 1, sizeof(char*));

	if(!argv)
		retError(clean, Error_outOfMemory(0, "Process_run() couldn't allocate argv"));

	argv[0] = Process_dupCStr(executable);
	Bool argsOk = argv[0] != NULL;

	for(U64 i = 0; args && i < args->length && argsOk; ++i) {
		argv[i + 1] = Process_dupCStr(args->ptr[i]);
		argsOk = argv[i + 1] != NULL;
	}

	if(!argsOk)
		retError(clean, Error_outOfMemory(0, "Process_run() couldn't allocate an argument"));

	if(workingDir && CharString_length(*workingDir)) {
		workDir = Process_dupCStr(*workingDir);
		if(!workDir)
			retError(clean, Error_outOfMemory(0, "Process_run() couldn't allocate working dir"));
	}

	if((stdOut && pipe(outFd)) || (stdErr && pipe(errFd)))
		retError(clean, Error_invalidState(0, "Process_run() couldn't create a pipe"));

	pid = fork();

	if(pid < 0)
		retError(clean, Error_invalidState(1, "Process_run() fork failed"));

	if(pid == 0) {

		//Child: redirect stdout/stderr into the pipes, drop the read ends, optionally chdir, then exec.
		//Only async-signal-safe calls here; on any failure _exit(127) so the parent sees a non-zero code.

		if(stdOut) { dup2(outFd[1], STDOUT_FILENO); close(outFd[0]); close(outFd[1]); }
		if(stdErr) { dup2(errFd[1], STDERR_FILENO); close(errFd[0]); close(errFd[1]); }

		if(workDir && chdir(workDir) != 0)
			_exit(127);

		execvp(argv[0], argv);
		_exit(127);
	}

	//Parent: close the write ends so the pipes reach EOF once the child (only remaining writer) exits

	if(outFd[1] >= 0) { close(outFd[1]); outFd[1] = -1; }
	if(errFd[1] >= 0) { close(errFd[1]); errFd[1] = -1; }

	const Ns start = Time_now();
	Bool timedOut = false;

	while(outFd[0] >= 0 || errFd[0] >= 0) {

		struct pollfd fds[2];
		int idx[2];
		nfds_t nfds = 0;

		if(outFd[0] >= 0) { fds[nfds].fd = outFd[0]; fds[nfds].events = POLLIN; idx[nfds] = 0; ++nfds; }
		if(errFd[0] >= 0) { fds[nfds].fd = errFd[0]; fds[nfds].events = POLLIN; idx[nfds] = 1; ++nfds; }

		int timeoutMs = -1;

		if(maxTimeout && maxTimeout != U64_MAX) {
			const Ns elapsed = Time_now() - start;
			timeoutMs = elapsed >= maxTimeout ? 0 : (int) U64_min((maxTimeout - elapsed) / MS, (Ns) I32_MAX);
		}

		const int r = poll(fds, nfds, timeoutMs);

		if(r < 0) {
			if(errno == EINTR)
				continue;
			retError(clean, Error_invalidState(0, "Process_run() poll failed"));
		}

		if(r == 0) {                                //Timed out: kill the child and drain whatever's left
			kill(pid, SIGKILL);
			timedOut = true;
		}

		for(nfds_t i = 0; i < nfds; ++i) {

			if(!(fds[i].revents & (POLLIN | POLLHUP | POLLERR)))
				continue;

			int *fd = idx[i] == 0 ? &outFd[0] : &errFd[0];
			ListU8 *acc = idx[i] == 0 ? &outAcc : &errAcc;

			U8 chunk[4096];
			const ssize_t got = read(*fd, chunk, sizeof(chunk));

			if(got <= 0) {                          //EOF or error: stop watching this pipe
				close(*fd);
				*fd = -1;
				continue;
			}

			if(acc->length + (U64) got > PROCESS_MAX_CAPTURE)
				retError(clean, Error_outOfBounds(
					0, acc->length + (U64) got, PROCESS_MAX_CAPTURE, "Process_run() captured output too large"
				));

			ListU8 ref = (ListU8) { 0 };
			gotoIfError3(clean, ListU8_createRefConst(chunk, (U64) got, &ref, e_rr));
			gotoIfError3(clean, ListU8_pushAll(acc, ref, alloc, e_rr));
		}
	}

	//Reap the child (also collects the exit status)

	int status = 0;
	while(waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
	pid = -1;

	I32 exitCode = -1;

	if(!timedOut && WIFEXITED(status))
		exitCode = (I32) WEXITSTATUS(status);

	if(result)
		*result = (ProcessResult) { .exitCode = exitCode, .timedOut = timedOut };

	if(stdOut && outAcc.length)
		gotoIfError3(clean, Buffer_createCopy(ListU8_bufferConst(outAcc), alloc, stdOut, e_rr));

	if(stdErr && errAcc.length)
		gotoIfError3(clean, Buffer_createCopy(ListU8_bufferConst(errAcc), alloc, stdErr, e_rr));

clean:

	if(outFd[0] >= 0) close(outFd[0]);
	if(outFd[1] >= 0) close(outFd[1]);
	if(errFd[0] >= 0) close(errFd[0]);
	if(errFd[1] >= 0) close(errFd[1]);

	//If we bailed before reaping (e.g. capture-too-large), don't leave a zombie or a runaway child

	if(pid > 0) {
		kill(pid, SIGKILL);
		int ignored = 0;
		while(waitpid(pid, &ignored, 0) < 0 && errno == EINTR) {}
	}

	if(argv) {
		
		for(U64 i = 0; i < argCount; ++i)
			free(argv[i]);

		free(argv);
	}

	free(workDir);

	ListU8_free(&outAcc, alloc);
	ListU8_free(&errAcc, alloc);
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

	gotoIfError3(clean, Process_spawn(resolved, args, workingDir, maxTimeout, stdOut, stdErr, result, e_rr));

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

	gotoIfError3(clean, Process_spawn(executable, args, workingDir, maxTimeout, stdOut, stdErr, result, e_rr));

clean:
	return s_uccess;
}

#endif
