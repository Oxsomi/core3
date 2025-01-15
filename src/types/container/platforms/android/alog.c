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
#include "types/container/buffer.h"
#include "types/container/string.h"
#include "types/base/error.h"
#include "types/base/allocator.h"

//Comparable to https://stackoverflow.com/questions/8115192/android-ndk-getting-the-backtrace

#include <unwind.h>
#include <dlfcn.h>
#include <android/log.h>

typedef struct Backtrace {
    void **current, **end;
} Backtrace;

_Unwind_Reason_Code unwindCallback(struct _Unwind_Context *context, Backtrace *state) {

    U64 pc = _Unwind_GetIP(context);

    if (state->current == state->end)
        return _URC_END_OF_STACK;

    *state->current++ = (void*)pc;

    if(!pc)
        return _URC_END_OF_STACK;

    return _URC_NO_REASON;
}

void Log_captureStackTrace(Allocator alloc, void **stack, U64 stackSize, U8 skipTmp) {

    U64 skip = (U64) skipTmp + 1;
    Backtrace backtrace = (Backtrace) { .current = stack, .end = &stack[stackSize] };
    _Unwind_Backtrace((_Unwind_Trace_Fn)unwindCallback, &backtrace);

    I32 count = backtrace.current - stack;

    if ((U32)count >= stackSize) {			//Call backTrace again, but this time we have to allocate

        U64 oldStackSize = stackSize;
        stackSize += skip;

        Buffer buf = Buffer_createNull();
        Error err = Buffer_createUninitializedBytes(stackSize * sizeof(void*), alloc, &buf);

        if (!err.genericError) {		//If allocate fails, we'll pretend that the stack ends after

            void **newStack = (void**) buf.ptrNonConst;

            backtrace = (Backtrace) { .current = newStack, .end = &newStack[stackSize] };
            _Unwind_Backtrace((_Unwind_Trace_Fn)unwindCallback, &backtrace);
            count = backtrace.current - stack;

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

void Log_log(Allocator alloc, ELogLevel lvl, ELogOptions options, CharString arg) {

    (void) alloc;

    if(lvl >= ELogLevel_Count)
        return;

    //[<thread>]: <ourStuff><\n if enabled>

    int androidLvl;

    switch(lvl) {
        default:                        androidLvl = ANDROID_LOG_DEBUG;     break;
	    case ELogLevel_Performance:     androidLvl = ANDROID_LOG_INFO;      break;
	    case ELogLevel_Warn:            androidLvl = ANDROID_LOG_WARN;      break;
	    case ELogLevel_Error:           androidLvl = ANDROID_LOG_ERROR;     break;
    }
    
    U64 thread = Thread_getId();
    const C8 *newLine = options & ELogOptions_NewLine ? "\n" : "";

    if(options & ELogOptions_Thread)
        __android_log_print(androidLvl, "OxC3", "[%"PRIu64"]: %.*s%s", thread, (int)CharString_length(arg), arg.ptr, newLine);

    else __android_log_print(androidLvl, "OxC3", "%.*s%s", (int)CharString_length(arg), arg.ptr, newLine);
}

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

	for(U64 i = 0; i < stackSize && stackTrace[i]; ++i) {

		Dl_info dlInfo = (Dl_info) { 0 };

		if(!dladdr(stackTrace[i], &dlInfo) || !dlInfo.dli_sname)
			Log_logFormat(alloc, lvl, opt, "%p", stackTrace[i]);

		else Log_logFormat(
			alloc,
			lvl,
			opt,
			dlInfo.dli_fname ? "%p: %s (%s)" : "%p: %s",
			stackTrace[i],
			dlInfo.dli_sname,
			dlInfo.dli_fname
		);
	}
}
