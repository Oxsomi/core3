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

//graphics/test/interface/test_graphics_refusing_stream.c

#include "test_graphics_shared.h"
#include "types/container/stream.h"
#include "types/base/error.h"

//In C rather than next to the test that uses it: the device calls a stream's read from C, and UBSan's function check
// only accepts a callee whose type is the C one, which a C++ definition never is.

U32 TestGraphics_refusedReads = 0;

Bool TestGraphics_refusingRead(OxStream *stream, U64 offset, U64 length, Buffer buf, const Allocator *alloc, Error *e_rr) {

	Bool s_uccess = true;

	(void) stream;
	(void) offset;
	(void) length;
	(void) buf;
	(void) alloc;

	++TestGraphics_refusedReads;
	retError(clean, Error_invalidState(0, "TestGraphics_refusingRead() refuses on purpose, to fail a recording"));

clean:
	return s_uccess;
}
