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

//types/container/test/failing_stream.h

#pragma once
#include "types/base/types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct RefPtr RefPtr;

//Makes one write to a stream fail, so a writer's error path can be tested: once armed, the failAt'th write to the
//stream (1 based) returns an error and every other write goes through. failAt 0 fails none and only counts, which is
// how a test learns how many writes a writer makes before failing each of them in turn.
//The stream stays armed for its lifetime. The counters are global, so one stream is armed at a time, as tests run
// serially.

void FailingStream_arm(RefPtr *stream, U64 failAt);

U64 FailingStream_writes();        //Writes seen since the last arm, the failed one included
Bool FailingStream_failed();       //Whether the armed write was reached and failed

#ifdef __cplusplus
	}
#endif
