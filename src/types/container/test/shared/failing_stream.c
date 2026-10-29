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

//types/container/test/shared/failing_stream.c

#include "types/container/test/failing_stream.h"
#include "types/container/stream.h"
#include "types/container/ref_ptr.h"
#include "types/base/error.h"

//Every stream armed so far is of a type whose write is the same function, so remembering the last one is enough.
//Arming a stream whose write is already the wrapper keeps the original instead of wrapping twice.

static StreamFunc FailingStream_original;
static U64 FailingStream_failAt;
static U64 FailingStream_count;
static Bool FailingStream_hit;

static Bool FailingStream_write(OxStream *stream, U64 offset, U64 length, Buffer buf, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	if (++FailingStream_count == FailingStream_failAt) {
		FailingStream_hit = true;
		retError(clean, Error_invalidState(0, "FailingStream_write() failed on purpose"));
	}

	gotoIfError3(clean, FailingStream_original(stream, offset, length, buf, alloc, e_rr));

clean:
	return s_uccess;
}

void FailingStream_arm(RefPtr *stream, U64 failAt) {

	OxStream *s = RefPtr_data(stream, OxStream);

	if(s->write != FailingStream_write) {
		FailingStream_original = s->write;
		s->write = FailingStream_write;
	}

	FailingStream_failAt = failAt;
	FailingStream_count = 0;
	FailingStream_hit = false;
}

U64 FailingStream_writes() { return FailingStream_count; }
Bool FailingStream_failed() { return FailingStream_hit; }
