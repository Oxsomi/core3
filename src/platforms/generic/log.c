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

//platforms/log.c

#include "platforms/log.h"
#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/base/constants.h"

#include <stdlib.h>

#define Log_level(lvl) 															\
																				\
	if(!format)																	\
		return;																	\
																				\
	CharString res = CharString_createNull();									\
																				\
	va_list arg1;																\
	va_start(arg1, format);														\
	Error err = Error_none();													\
	Bool s_uccess = CharString_formatVariadic(alloc, &res, &err, format, arg1);	\
	va_end(arg1);																\
																				\
	if(!s_uccess)																\
		Log_log(alloc, lvl, opt, &res);											\
																				\
	CharString_free(&res, alloc)

//Default allocator. Sometimes they can't be safely used

void Log_logFormatx(ELogLevel level, ELogOptions opt, const C8 *format, ...) {
	const Allocator *alloc = Platform_instance->alloc;
	Log_level(level);
}

void Log_debugx(ELogOptions opt, const C8 *format, ...) {
	const Allocator *alloc = Platform_instance->alloc;
	Log_level(ELogLevel_Debug);
}

void Log_performancex(ELogOptions opt, const C8 *format, ...) {
	const Allocator *alloc = Platform_instance->alloc;
	Log_level(ELogLevel_Performance);
}

void Log_warnx(ELogOptions opt, const C8 *format, ...) {
	const Allocator *alloc = Platform_instance->alloc;
	Log_level(ELogLevel_Warn);
}

void Log_errorx(ELogOptions opt, const C8 *format, ...) {
	const Allocator *alloc = Platform_instance->alloc;
	Log_level(ELogLevel_Error);
}
