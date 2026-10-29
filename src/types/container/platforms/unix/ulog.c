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

//types/container/platforms/unix/ulog.c

#define _GNU_SOURCE

#include "types/container/log.h"
#include "types/base/thread.h"
#include "types/base/time.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/string_read_helper.h"
#include "types/base/error.h"
#include "types/base/allocator.h"
#include "types/base/lock.h"

//Required for the _PLATFORM_TYPE / PLATFORM_* checks below.
//Without it the preprocessor silently treats all of them as 0, the ANDROID/IOS exclusion below excludes *every* platform
// and Log_log is never compiled (surfacing as "undefined reference to Log_log" when linking on Linux/OSX).
#include "types/base/platform_types.h"

#include <stdio.h>
#include <stdlib.h>

CharString Error_formatPlatformError(const Allocator *alloc, const Error *e_rr) {
	(void) alloc; (void) e_rr;
	return CharString_createNull();
}

#if _PLATFORM_TYPE != PLATFORM_ANDROID &&  _PLATFORM_TYPE != PLATFORM_IOS

	#include <dlfcn.h>
	#include <unistd.h>
	#include <string.h>
	#include <stdio.h>

	#if _PLATFORM_TYPE != PLATFORM_WEB
		#include <errno.h>
		#include <fcntl.h>
		#include <spawn.h>
		#include <sys/wait.h>
		extern char **environ;
	#endif

	#ifdef __APPLE__
		#include <mach-o/dyld.h>
	#endif

	#if _PLATFORM_TYPE == PLATFORM_WEB

		//A frame on web is a pointer into the one callstack string emscripten hands back, at the start of its
		// line (see the web branch of types/base/platforms/unix/uerror.c).
		//dladdr would be meaningless on those, so this prints each line as text instead, stopping at the newline
		// that starts the next frame.
		//emscripten already formats them as "    at <symbol> (<file>:<line>:<col>)", so the shape matches what
		// addr2line produces below closely enough to read the same way.

		void Log_printCapturedStackTraceCustom(
			const Allocator *alloc,
			const void **stackTrace,
			U64 stackSize,
			ELogLevel lvl,
			ELogOptions opt
		) {
			if(!stackTrace || lvl >= ELogLevel_Count)
				return;

			//stackTrace[0] is the ring generation, not a frame.
			//If the ring recycled that slot the text now belongs to some later capture, so say the trace is
			// gone rather than print an unrelated stack as if it were this one.

			if(!Error_webStackTraceIsLive(stackTrace)) {
				Log_logFormat(alloc, lvl, opt, "Stacktrace: expired (the web trace ring recycled this slot)");
				return;
			}

			Log_logFormat(alloc, lvl, opt, "Stacktrace:");

			for(U64 i = 1; i < stackSize && stackTrace[i]; ++i) {

				const C8 *line = (const C8*) stackTrace[i];
				U64 len = 0;

				while(line[len] && line[len] != '\n')
					++len;

				const CharString frame = CharString_createRefSizedConst(line, len, false);
				Log_log(alloc, lvl, opt, &frame);
			}
		}

	#else

	static FILE *Log_openSymbolizer(C8 *const *arguments, pid_t *process) {

		int descriptors[2];

		if(pipe(descriptors))
			return NULL;

		for(U64 i = 0; i < 2; ++i) {

			if(descriptors[i] < 3) {

				const int original = descriptors[i];

				descriptors[i] = fcntl(original, F_DUPFD, 3);
				close(original);
			}

			if(descriptors[i] < 0 || fcntl(descriptors[i], F_SETFD, FD_CLOEXEC)) {

				close(descriptors[0]);
				close(descriptors[1]);
				return NULL;
			}
		}

		posix_spawn_file_actions_t actions;

		int result = posix_spawn_file_actions_init(&actions);

		if(!result) {

			result = posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDOUT_FILENO);

			if(!result)
				result = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

			if(!result)
				result = posix_spawnp(process, arguments[0], &actions, NULL, arguments, environ);

			posix_spawn_file_actions_destroy(&actions);
		}

		close(descriptors[1]);

		FILE *output = result ? NULL : fdopen(descriptors[0], "r");

		if(!output) {

			close(descriptors[0]);

			if(!result)
				while(waitpid(*process, NULL, 0) < 0 && errno == EINTR)
					;
		}

		return output;
	}

	void Log_printCapturedStackTraceCustom(
		const Allocator *alloc,
		const void **stackTrace,
		U64 stackSize,
		ELogLevel lvl,
		ELogOptions opt
	) {
		if(!stackTrace || lvl >= ELogLevel_Count)
			return;

		Log_logFormat(alloc, lvl, opt, "Stacktrace:");

		//Group frames by module so we can batch addr2line calls per module.
		//With 32 frames max, a simple O(n^2) grouping is fine.

		U64 count = 0;

		for(; count < stackSize && count < 64 && stackTrace[count]; ++count)
			;

		//Collect unique modules
		const C8 *modules[64] = { 0 };
		U64 moduleCount = 0;

		for(U64 i = 0; i < count; ++i) {
			Dl_info info = (Dl_info) { 0 };
			dladdr(stackTrace[i], &info);
			const C8 *mod = info.dli_fname ? info.dli_fname : "";

			const CharString modStr = CharString_createRefCStrConst(mod);

			Bool found = false;

			for(U64 m = 0; m < moduleCount; ++m)
				if(CharString_equalsCStringSensitive(&modStr, modules[m])) { found = true; break; }

			if(!found)
				modules[moduleCount++] = mod;
		}

		//One addr2line/atos call per unique module, capturing all frames for it
		C8 output[64][256];    //per-frame resolved string, filled in by module pass
		U64 resolved = 0;

		for(U64 m = 0; m < moduleCount; ++m) {

			//Pass module paths as arguments so shell syntax and path length cannot alter the command.
			C8 *arguments[71] = { 0 };
			C8 addresses[64][32];
			U64 argumentCount = 0;
			U64 frameIndices[64];
			U64 frameCount = 0;

			#if _PLATFORM_TYPE == PLATFORM_OSX
				//atos: atos -o <module> -l <base> <addr> <addr> ...
				//We need dli_fbase for the load address
				void *base = NULL;

				for(U64 i = 0; i < count; ++i) {
					Dl_info info = (Dl_info){ 0 };
					dladdr(stackTrace[i], &info);
					const C8 *mod = info.dli_fname ? info.dli_fname : "";
					const CharString modStr = CharString_createRefCStrConst(mod);

					if(CharString_equalsCStringSensitive(&modStr, modules[m])) {
						base = info.dli_fbase;
						break;
					}
				}

				C8 baseAddress[32];

				snprintf(baseAddress, sizeof(baseAddress), "%p", base);
				arguments[argumentCount++] = "atos";
				arguments[argumentCount++] = "-o";
				arguments[argumentCount++] = (C8*) modules[m];
				arguments[argumentCount++] = "-l";
				arguments[argumentCount++] = baseAddress;
			#else
				arguments[argumentCount++] = "addr2line";
				arguments[argumentCount++] = "-Cpe";
				arguments[argumentCount++] = (C8*) modules[m];
			#endif

			for(U64 i = 0; i < count; ++i) {

				Dl_info info = (Dl_info){ 0 };
				dladdr(stackTrace[i], &info);
				const C8 *mod = info.dli_fname ? info.dli_fname : "";
				const CharString modStr = CharString_createRefCStrConst(mod);

				if(!CharString_equalsCStringSensitive(&modStr, modules[m]))
					continue;

				#ifdef __APPLE__
					snprintf(addresses[frameCount], sizeof(addresses[0]), "%p", stackTrace[i]);
				#else
					uintptr_t offset = (uintptr_t) stackTrace[i] - (uintptr_t) info.dli_fbase;
					snprintf(addresses[frameCount], sizeof(addresses[0]), "0x%"PRIxPTR, offset);
				#endif

				arguments[argumentCount++] = addresses[frameCount];
				frameIndices[frameCount++] = i;
			}

			pid_t process;

			FILE *fp = Log_openSymbolizer(arguments, &process);

			if(!fp)
				continue;

			//Both tools produce one line per address with these options.

			for(U64 f = 0; f < frameCount; ++f) {

				C8 *line = output[frameIndices[f]];

				if(!fgets(line, sizeof(output[0]), fp))
					break;

				const U64 length = CharString_calcStrLen(line, sizeof(output[0]));

				if(length && line[length - 1] == '\n')
					line[length - 1] = '\0';

				else {

					int next;

					while((next = fgetc(fp)) != '\n' && next != EOF)
						;
				}

				if(line[0] && line[0] != '?')
					resolved |= ((U64) 1 << frameIndices[f]);
			}

			fclose(fp);

			while(waitpid(process, NULL, 0) < 0 && errno == EINTR)
				;
		}

		//Print all frames in order, using resolved output or dladdr fallback
		for(U64 i = 0; i < count; ++i) {

			if((resolved >> i) & 1) {
				Log_logFormat(alloc, lvl, opt | ELogOptions_NewLine, "%s", output[i]);
				continue;
			}

			//dladdr fallback
			Dl_info info = (Dl_info){ 0 };
			if(dladdr(stackTrace[i], &info) && info.dli_sname)
				Log_logFormat(alloc, lvl, opt | ELogOptions_NewLine,
					info.dli_fname ? "%p: %s (%s)" : "%p: %s",
					stackTrace[i], info.dli_sname, info.dli_fname
				);

			else Log_logFormat(alloc, lvl, opt | ELogOptions_NewLine, "%p", stackTrace[i]);
		}
	}

	#endif

	#define FONT_GREEN  "\e[1;32m"
	#define FONT_CYAN   "\e[1;36m"
	#define FONT_YELLOW "\e[1;33m"
	#define FONT_RED    "\e[1;31m"
	#define FONT_RESET  "\e[1;0m"

	#define printColor(lvl, str, ...)                                                                     \
		switch(lvl) {                                                                                     \
			default:                       printf(FONT_GREEN  str FONT_RESET, __VA_ARGS__);        break;    \
			case ELogLevel_Performance:    printf(FONT_CYAN   str FONT_RESET, __VA_ARGS__);        break; \
			case ELogLevel_Warn:           printf(FONT_YELLOW str FONT_RESET, __VA_ARGS__);        break;    \
			case ELogLevel_Error:          printf(FONT_RED    str FONT_RESET, __VA_ARGS__);        break;   \
		}

	#define printColorSimple(lvl, str)                                                                       \
		switch(lvl) {                                                                                        \
			default:                       printf(FONT_GREEN  str FONT_RESET);        break;                    \
			case ELogLevel_Performance:    printf(FONT_CYAN   str FONT_RESET);        break;                 \
			case ELogLevel_Warn:           printf(FONT_YELLOW str FONT_RESET);        break;                    \
			case ELogLevel_Error:          printf(FONT_RED    str FONT_RESET);        break;                   \
		}

	static SpinLock LogPrettyPrintLock = { 0 };

	void Log_log(const Allocator *alloc, ELogLevel lvl, ELogOptions options, const CharString *arg) {

		(void) alloc;

		Ns t = Time_now();

		if(lvl >= ELogLevel_Count)
			return;

		const ELockAcquire acq = SpinLock_lock(&LogPrettyPrintLock, U64_MAX);

		if(acq < ELockAcquire_Success)
			return;

		//[<thread> <time>]: <hr\n><ourStuff> <\n if enabled>

		Bool hasTimestamp = options & ELogOptions_Timestamp;
		Bool hasThread = options & ELogOptions_Thread;
		Bool hasNewLine = options & ELogOptions_NewLine;
		Bool hasPrepend = hasTimestamp || hasThread;

		if (hasPrepend)
			printColorSimple(lvl, "[");

		if (hasThread)
			printColor(lvl, "%"PRIu64, Thread_getId());

		if (hasTimestamp) {

			TimeFormat tf;
			Time_format(t, tf, true);

			printColor(lvl, "%s%s", hasThread ? " " : "", tf);
		}

		if (hasPrepend)
			printColorSimple(lvl, "]: ");

		//Print to console and debug window

		const C8 *newLine = hasNewLine ? "\n" : "";

		printColor(lvl,
			"%.*s%s",
			!arg ? 0 : (int) CharString_length(*arg), !arg ? "" : arg->ptr,
			newLine
		);

		if(acq == ELockAcquire_Acquired)
			SpinLock_unlock(&LogPrettyPrintLock);
	}

#endif
