/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2025 Oxsomi / Nielsbishere (Niels Brunekreef)
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
#include "types/base/thread.h"
#include "types/base/time.h"
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/error.h"
#include "types/base/allocator.h"

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

CharString Error_formatPlatformError(Allocator alloc, Error err) { (void) alloc; (void)err; return CharString_createNull(); }

#if _PLATFORM_TYPE != PLATFORM_ANDROID

	#include <execinfo.h>

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

		Log_logFormat(alloc, lvl, opt, "Stacktrace:\n");

		U64 i = 0;

		for(; i < stackSize && stackTrace[i]; ++i)		//Find end
			;

		C8 **symbols = backtrace_symbols((void* const *)stackTrace, i);

		for(U64 j = 0; j < i; ++j)
			Log_logFormat(
				alloc,
				lvl,
				opt | ELogOptions_NewLine,
				"%s",
				symbols[j]
			);

		free(symbols);
	}

	void Log_captureStackTrace(Allocator alloc, void **stack, U64 stackSize, U8 skipTmp) {

		U64 skip = (U64) skipTmp + 1;

		I32 count = backtrace(stack, stackSize);

		if ((U32)count >= stackSize) {			//Call backTrace again, but this time we have to allocate

			U64 oldStackSize = stackSize;
			stackSize += skip;

			Buffer buf = Buffer_createNull();
			Error err = Buffer_createUninitializedBytes(stackSize * sizeof(void*), alloc, &buf);

			if (!err.genericError) {		//If allocate fails, we'll pretend that the stack ends after

				void **newStack = (void**) buf.ptrNonConst;

				I32 count = backtrace(newStack, stackSize);

				for (U64 i = 0; i < oldStackSize && newStack[i + skip]; ++i)
					stack[i] = newStack[i + skip];

				if((U64)(count - skip) < oldStackSize)
					stack[(U64)(count - skip)] = NULL;

				Buffer_free(&buf, alloc);
				return;
			}

			stackSize = oldStackSize;		//Restore, apparently can't allocate, so empty elements after
		}

		//Skip part of stack

		for (U64 i = skip; i < stackSize && stack[i]; ++i)
			stack[i - skip] = stack[i];

		if((U64)(count - skip) < stackSize)
			stack[(U64)(count - skip)] = NULL;
	}

	#define FONT_GREEN  "\e[1;32m"
	#define FONT_CYAN   "\e[1;36m"
	#define FONT_YELLOW "\e[1;33m"
	#define FONT_RED    "\e[1;31m"
	#define FONT_RESET  "\e[1;0m"

	#define printColor(lvl, str, ...)																	\
		switch(lvl) {																					\
			default:					printf(FONT_GREEN  str FONT_RESET, __VA_ARGS__);		break;	\
			case ELogLevel_Performance:	printf(FONT_CYAN   str FONT_RESET, __VA_ARGS__);		break;	\
			case ELogLevel_Warn:		printf(FONT_YELLOW str FONT_RESET, __VA_ARGS__);		break;	\
			case ELogLevel_Error:		printf(FONT_RED    str FONT_RESET, __VA_ARGS__);		break;	\
		}

	#define printColorSimple(lvl, str)																	\
		switch(lvl) {																					\
			default:					printf(FONT_GREEN  str FONT_RESET);		break;					\
			case ELogLevel_Performance:	printf(FONT_CYAN   str FONT_RESET);		break;					\
			case ELogLevel_Warn:		printf(FONT_YELLOW str FONT_RESET);		break;					\
			case ELogLevel_Error:		printf(FONT_RED    str FONT_RESET);		break;					\
		}

	void Log_log(Allocator alloc, ELogLevel lvl, ELogOptions options, CharString arg) {

		(void) alloc;

		Ns t = Time_now();

		if(lvl >= ELogLevel_Count)
			return;

		U64 thread = Thread_getId();

		//[<thread> <time>]: <hr\n><ourStuff> <\n if enabled>

		Bool hasTimestamp = options & ELogOptions_Timestamp;
		Bool hasThread = options & ELogOptions_Thread;
		Bool hasNewLine = options & ELogOptions_NewLine;
		Bool hasPrepend = hasTimestamp || hasThread;

		if (hasPrepend)
			printColorSimple(lvl, "[");

		if (hasThread)
			printColor(lvl, "%"PRIu64, thread);

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
			(int)CharString_length(arg), arg.ptr,
			newLine
		);
	}

#endif
