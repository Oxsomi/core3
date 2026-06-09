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
#include "types/base/error.h"
#include "types/base/allocator.h"

#include <stdio.h>
#include <stdlib.h>

CharString Error_formatPlatformError(const Allocator *alloc, const Error *e_rr) {
	(void) alloc; (void)e_rr;
	return CharString_createNull();
}

#if _PLATFORM_TYPE != PLATFORM_ANDROID &&  _PLATFORM_TYPE != PLATFORM_IOS

	#include <dlfcn.h>
	#include <unistd.h>
	#include <string.h>
	#include <stdio.h>

	#ifdef __APPLE__
		#include <mach-o/dyld.h>
	#endif

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

			Bool found = false;
			for(U64 m = 0; m < moduleCount; ++m)
				if(strcmp(modules[m], mod) == 0) { found = true; break; }

			if(!found)
				modules[moduleCount++] = mod;
		}

		//One addr2line/atos call per unique module, capturing all frames for it
		C8 output[64][256];    //per-frame resolved string, filled in by module pass
		U64 resolved = 0;

		for(U64 m = 0; m < moduleCount; ++m) {

			//Build command with all addresses belonging to this module
			C8 cmd[4096];
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
					if(strcmp(mod, modules[m]) == 0) {
						base = info.dli_fbase;
						break;
					}
				}

				int written = snprintf(cmd, sizeof(cmd), "atos -o \"%s\" -l %p", modules[m], base);
			#else
				int written = snprintf(cmd, sizeof(cmd), "addr2line -Cpe \"%s\"", modules[m]);
			#endif

			for(U64 i = 0; i < count && written < (int)sizeof(cmd) - 32; ++i) {
				Dl_info info = (Dl_info){ 0 };
				dladdr(stackTrace[i], &info);
				const C8 *mod = info.dli_fname ? info.dli_fname : "";
				if(strcmp(mod, modules[m]) != 0)
					continue;

				#ifdef __APPLE__
					written += snprintf(cmd + written, sizeof(cmd) - written, " %p", stackTrace[i]);
				#else
					uintptr_t offset = (uintptr_t)stackTrace[i] - (uintptr_t)info.dli_fbase;
					written += snprintf(cmd + written, sizeof(cmd) - written, " 0x%"PRIxPTR, offset);
				#endif

				frameIndices[frameCount++] = i;
			}

			snprintf(cmd + written, sizeof(cmd) - written, " 2>/dev/null");

			FILE *fp = popen(cmd, "r");
			if(!fp)
				continue;

			//addr2line outputs one or two lines per address (function, then file:line)
			//atos outputs one line per address

			for(U64 f = 0; f < frameCount; ++f) {

				#if _PLATFORM_TYPE == PLATFORM_OSX

					C8 line[256] = { 0 };

					if(!fgets(line, sizeof(line), fp))
						break;
					U64 len = CharString_calcStrLen(line, sizeof(line));
					if(len && line[len-1] == '\n') line[len-1] = '\0';
					if(line[0] && line[0] != '?') {
						snprintf(output[frameIndices[f]], sizeof(output[0]), "%s", line);
						resolved |= ((U64)1 << frameIndices[f]);
					}

				#else

					if(!fgets(output[frameIndices[f]], sizeof(output[0]), fp))
						break;

					U64 len = CharString_calcStrLen(output[frameIndices[f]], sizeof(output[0]));
					if(len && output[frameIndices[f]][len-1] == '\n')
						output[frameIndices[f]][len-1] = '\0';

					if(output[frameIndices[f]][0] && output[frameIndices[f]][0] != '?')
						resolved |= ((U64)1 << frameIndices[f]);

					else output[frameIndices[f]][0] = '\0';
					
				#endif
			}

			pclose(fp);
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

	#define FONT_GREEN  "\e[1;32m"
	#define FONT_CYAN   "\e[1;36m"
	#define FONT_YELLOW "\e[1;33m"
	#define FONT_RED    "\e[1;31m"
	#define FONT_RESET  "\e[1;0m"

	#define printColor(lvl, str, ...)                                                                    \
		switch(lvl) {                                                                                    \
			default:                    printf(FONT_GREEN  str FONT_RESET, __VA_ARGS__);        break;    \
			case ELogLevel_Performance:    printf(FONT_CYAN   str FONT_RESET, __VA_ARGS__);        break;    \
			case ELogLevel_Warn:        printf(FONT_YELLOW str FONT_RESET, __VA_ARGS__);        break;    \
			case ELogLevel_Error:        printf(FONT_RED    str FONT_RESET, __VA_ARGS__);        break;    \
		}

	#define printColorSimple(lvl, str)                                                                    \
		switch(lvl) {                                                                                    \
			default:                    printf(FONT_GREEN  str FONT_RESET);        break;                    \
			case ELogLevel_Performance:    printf(FONT_CYAN   str FONT_RESET);        break;                    \
			case ELogLevel_Warn:        printf(FONT_YELLOW str FONT_RESET);        break;                    \
			case ELogLevel_Error:        printf(FONT_RED    str FONT_RESET);        break;                    \
		}

	void Log_log(const Allocator *alloc, ELogLevel lvl, ELogOptions options, const CharString *arg) {

		(void) alloc;

		Ns t = Time_now();

		if(lvl >= ELogLevel_Count)
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
			!arg ? 0 : (int)CharString_length(*arg), !arg ? "" : arg->ptr,
			newLine
		);
	}

#endif
