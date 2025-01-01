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

#include "types/container/buffer.h"
#include "platforms/ext/errorx.h"
#include "platforms/ext/stringx.h"
#include "platforms/log.h"
#include "platforms/platform.h"

void Error_fillStackTrace(Error *err) {

	//Skip Error_fillStackTrace (skip=1), Error_x (skip=2)

	if(err)
		Log_captureStackTracex(err->stackTrace, ERROR_STACKTRACE, 2);
}

void Error_printx(Error err, ELogLevel logLevel, ELogOptions options) {
	Error_print(Platform_instance->alloc, err, logLevel, options);
}

void Error_printLnx(Error err) {
	Error_printx(err, ELogLevel_Error, ELogOptions_Default);
}
