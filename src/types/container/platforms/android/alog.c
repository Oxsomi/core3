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

//types/container/platforms/android/alog.c

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

void Log_log(const Allocator *alloc, ELogLevel lvl, ELogOptions options, const CharString *arg) {

	(void) alloc;

	if(lvl >= ELogLevel_Count)
		return;

	//[<thread>]: <ourStuff><\n if enabled>

	int androidLvl;

	switch(lvl) {
		default:                        androidLvl = ANDROID_LOG_DEBUG;     break;
		case ELogLevel_Performance:         androidLvl = ANDROID_LOG_INFO;      break;
		case ELogLevel_Warn:            androidLvl = ANDROID_LOG_WARN;      break;
		case ELogLevel_Error:               androidLvl = ANDROID_LOG_ERROR;     break;
	}
	
	U64 thread = Thread_getId();
	const C8 *newLine = options & ELogOptions_NewLine ? "\n" : "";

	if(options & ELogOptions_Thread)
		__android_log_print(
			androidLvl, "OxC3", "[%"PRIu64"]: %.*s%s",
			thread, !arg ? 0 : (int)CharString_length(*arg), !arg ? "" : arg->ptr, newLine
		);

	else __android_log_print(
		androidLvl, "OxC3", "%.*s%s", !arg ? 0 : (int)CharString_length(*arg), !arg ? "" : arg->ptr, newLine
	);
}

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
