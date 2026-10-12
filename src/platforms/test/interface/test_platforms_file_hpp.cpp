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

//platforms/test/interface/test_platforms_file_hpp.cpp
//
//Runs platforms/file.hpp's FileHandle, where test_platforms_hpp.cpp only compiles it: a file written, then read back
//whole, from an offset to its end and as a range inside it, with a read past the end refused.

#include "platforms/file.hpp"

//The C test framework carries an extern "C" guard, so it is included inside oxc::c after the wrapper, which already
// pulled its C header dependencies in there (test_types_container_hpp.cpp does the same).

namespace oxc { namespace c {
	#include "types/test/test.h"
}}

extern "C" void Test_fileHandleHpp(oxc::c::Test *t) {

	using namespace oxc;

	Test_setModule(t, "File/HandleHpp");

	const c::Allocator &alloc = *t->alloc;
	c::Error *e_rr = &t->err;

	const c::C8 payload[] = "0123456789abcdef";
	const c::U64 length = sizeof(payload) - 1;
	const StringView loc("platform_test_hpp_read.bin");

	file::Types types(&alloc);
	Buffer whole(alloc), tail(alloc), middle(alloc), past(alloc);
	FileHandle handle;

	Test_assert(t, "write", file::write(c::Buffer_createRefConst(payload, length), loc, types, 0, 0, 0, true, e_rr));

	if(Test_assert(t, "open", FileHandle::open(loc, types, handle, c::EFileOpenType_Read, false, e_rr))) {

		Test_assert(t, "readWhole", handle.read(whole, 0, 0, e_rr) && c::Buffer_eq(
			whole.handle(), c::Buffer_createRefConst(payload, length)
		));

		Test_assert(t, "readToEnd", handle.read(tail, 10, 0, e_rr) && c::Buffer_eq(
			tail.handle(), c::Buffer_createRefConst(payload + 10, length - 10)
		));

		Test_assert(t, "readRange", handle.read(middle, 4, 6, e_rr) && c::Buffer_eq(
			middle.handle(), c::Buffer_createRefConst(payload + 4, 6)
		));

		Test_assert(t, "readPastEndFails", !handle.read(past, length, 1));
		Test_assert(t, "refusedReadLeavesNothing", !c::Buffer_length(past.handle()));

		//Refused on the range rather than on allocating it.

		c::Error rangeErr = c::Error_none();

		Test_assert(
			t, "hugeRangeRefusedOnBounds",
			!handle.read(past, 0, c::U64_MAX / 2, &rangeErr) && rangeErr.genericError == c::EGenericError_OutOfBounds
		);

		handle.release();
	}

	Test_assert(t, "remove", file::remove(loc, &alloc, e_rr));
}
