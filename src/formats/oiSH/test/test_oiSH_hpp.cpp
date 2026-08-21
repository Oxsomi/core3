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

//formats/oiSH/test/test_oiSH_hpp.cpp
//
//Type check for the C++ oiSH layer.
//formats/oiSH.hpp is hand written and, until this file existed, no translation unit included it,
// so nothing had ever compiled it.
//This TU exists so the header is built by every configuration the tests are,
// which is what stops it drifting from the C API it wraps.
//
//The body below is deliberately never CALLED.
//It names every member of the wrapper so the compiler has to instantiate and typecheck it against the current
// C headers.
//Running it would need a real oiSH binary, which the suites in this same executable already read.
//A link time reference is enough to keep it honest.
//
//It is not registered in test_oiSH_main.c on purpose:
// a compile time check has nothing to assert at runtime.

#include "formats/oiSH.hpp"

//Never invoked.
//See the file comment: this is a compile time check, not a test module.

extern "C" void Test_oiSHHppTypeCheck(const oxc::c::Allocator *alloc) {

	using namespace oxc;

	c::Error *e_rr = nullptr;

	//The stream side comes first.
	//oiSH.hpp pulls memory_stream.hpp rather than the file API precisely because a shader binary can come from
	// anywhere that produces a stream, so the stream wrapper is part of what this header commits to.

	const MemoryStreamType streamType(alloc);

	Buffer bytes(*alloc);
	(void) bytes.createEmptyBytes(64, e_rr);

	MemoryStream stream;
	(void) MemoryStream::fromBufferRegion(bytes, streamType, stream, 0, 0, e_rr);

	//Construction and the two read forms, the second leaning on both default arguments.

	SHFile file(*alloc);

	c::U64 off = 0;
	(void) file.read(stream, &off, false, e_rr);
	(void) file.read(stream, &off);

	//Both handle() overloads, since C consumers take the mutable one and const callers the other.

	c::SHFile &mutableHandle = file.handle();
	(void) mutableHandle;

	const SHFile &constFile = file;
	const c::SHFile &constHandle = constFile.handle();
	(void) constHandle;

	//release() is public because a hot reload refills an SHFile rather than rebuilding one.

	file.release();

	//Move only, by design.
	//static_cast rather than a standard library move, this layer does not pull the STL in.

	SHFile moved(static_cast<SHFile&&>(file));
	SHFile target(*alloc);
	target = static_cast<SHFile&&>(moved);
}
