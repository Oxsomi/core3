/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2024 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "types/error.h"
#include "types/string.h"
#include "types/log.h"

#ifdef __cplusplus
	extern "C" {
#endif

void Log_captureStackTracex(void **stackTrace, U64 stackSize, U8 skip);
void Log_printCapturedStackTraceCustomx(const void **stackTrace, U64 stackSize, ELogLevel lvl, ELogOptions options);
void Log_logx(ELogLevel lvl, ELogOptions options, CharString arg);
void Log_printCapturedStackTracex(const StackTrace stackTrace, ELogLevel lvl, ELogOptions options);
void Log_printStackTracex(U8 skip, ELogLevel lvl, ELogOptions options);

//IMPORTANT:
//NEVER! Supply user generated content into format. Instead use "%.*s".
//When displaying strings, use "%.*s", args.length, arg.ptr instead of args.ptr, because strings aren't null terminated.
//(Only exception is if the strings are safely generated from code and are determined to be null terminated, then use %s)

void Log_debugx(ELogOptions options, const C8 *format, ...);
void Log_performancex(ELogOptions options, const C8 *format, ...);
void Log_warnx(ELogOptions options, const C8 *format, ...);
void Log_errorx(ELogOptions options, const C8 *format, ...);

#define Log_debugLnx(...)				Log_debugx(ELogOptions_NewLine, __VA_ARGS__)
#define Log_performanceLnx(...)			Log_performancex(ELogOptions_NewLine, __VA_ARGS__)
#define Log_warnLnx(...)				Log_warnx(ELogOptions_NewLine, __VA_ARGS__)
#define Log_errorLnx(...)				Log_errorx(ELogOptions_NewLine, __VA_ARGS__)

#ifdef __cplusplus
	}
#endif
