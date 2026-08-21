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

//types/container/test/test_types_container_hpp_memory_stream.cpp
//
//Type check for the C++ MemoryStream wrapper.
//types/container/memory_stream.hpp is hand written, and only formats/oiSH.hpp and platforms/file.hpp include it,
// neither of which any translation unit pulls in, so nothing had ever compiled it.
//This TU builds it in every configuration the tests are built in, which is what stops it drifting from the C it wraps.
//
//The body below is deliberately never CALLED.
//It names every member so the compiler has to typecheck each one against the current C headers.
//Running it would need a live allocator and a buffer that outlives the stream, which
// test_types_container_memory_stream.c already covers; a link time reference is enough to keep this honest.

#include "types/container/memory_stream.hpp"

//Never invoked. See the file comment: this is a compile time check, not a test module.

extern "C" void Test_hppMemoryStreamTypeCheck(const oxc::c::Allocator *alloc, oxc::c::Error *e_rr) {

	using namespace oxc;

	//The RefPtrType every stream is created through, which has to outlive all of them.

	MemoryStreamType types(alloc);
	(void) types.type.typeId;

	//The bytes the stream borrows; it does not own them, so this must outlive every copy below.

	Buffer bytes(*alloc);

	//Default construction, then the factory in both its defaulted and its fully spelled out form.

	MemoryStream stream;
	(void) MemoryStream::fromBufferRegion(bytes, types, stream);
	(void) MemoryStream::fromBufferRegion(bytes, types, stream, 16, 32, e_rr);

	//Copyable on purpose: a copy is one more reference, which is what the C refcount is for.

	MemoryStream copy = stream;
	MemoryStream assigned;
	assigned = copy;

	(void) stream.valid();
	(void) (bool) copy;
	(void) assigned.data();

	//MemoryStreamRef and StreamRef are the same bare RefPtr typedef,
	// so one handle() serves every C stream consumer without a cast.

	c::StreamRef *asStream = assigned.handle();
	(void) asStream;

	assigned.release();
	copy.release();
	stream.release();
}
