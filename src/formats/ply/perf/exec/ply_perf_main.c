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

//formats/ply/perf/exec/ply_perf_main.c
//
//Usage: OxC3_formats_ply_perf <path to .ply> [threadCount]
//threadCount defaults to Platform_getThreads() when omitted or 0.

#include "formats/ply/perf/ply_perf.h"
#include "platforms/platform.h"
#include "types/container/string.h"
#include "types/container/log.h"
#include "types/base/string_read.h"
#include "types/base/error.h"

Platform_defineEntrypoint() {

	int status = 0;
	Error err = Error_none();

	Bool s_uccess = Platform_create(Platform_argc, Platform_argv, Platform_getData(), NULL, true, NULL);

	if(!s_uccess)
		Platform_return(-2);

	if(!Platform_instance->args.length) {
		Log_errorLn(Platform_instance->alloc, "Usage: OxC3_formats_ply_perf <path to .ply> [threadCount]");
		status = -1;
		goto clean;
	}

	U64 threadCount = 0;

	if(Platform_instance->args.length >= 2)
		CharString_parseU64(Platform_instance->args.ptr[1], &threadCount);

	if(!Perf_plyParallelRead(Platform_instance->args.ptr, threadCount, Platform_instance->alloc, &err)) {
		Error_print(Platform_instance->alloc, &err, ELogLevel_Error, ELogOptions_Default);
		status = -1;
		goto clean;
	}

clean:
	Platform_cleanup();
	Platform_return(status);
}
