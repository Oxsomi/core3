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

//types/container/memory_stream.hpp
//
//C++ layer over MemoryStream.
//Lives here rather than beside the file API because a memory stream is a types/container object:
// it wraps borrowed bytes and knows nothing about the filesystem.
//The oiSH reader and the file loader both consume StreamRef,
// which is what made it tempting to file this under whichever of them happened to need it first.

#pragma once

//Existing wrappers first:
// they pull their C headers into oxc::c with the right pre-include discipline. string.hpp matters even though nothing
// here takes a CharString,
// it is what gets <stdarg.h> seen at global scope before any C header can drag va_list into the namespace.
#include "types/container/ref_ptr.hpp"
#include "types/container/string.hpp"
#include "types/container/buffer.hpp"

//vec4f.hpp must lead when _ENABLE_SIMD is on:
// the SSE path includes <emmintrin.h>, and an intrinsics header may not first appear inside a namespace.
#include "types/math/vec4f.hpp"

namespace oxc {

	namespace c {
		#include "types/container/memory_stream.h"
	}

	//The RefPtrType memory streams are created through.
	//Every RefPtr keeps pointing at its RefPtrType, so this must outlive every MemoryStream made from it.

	struct MemoryStreamType {

		c::RefPtrType type;

		explicit MemoryStreamType(const c::Allocator *alloc) noexcept : type(c::MemoryStream_makeType(alloc)) {}
	};

	//Thin, COPYABLE wrapper over oxc::RefPtr<c::MemoryStream>; the factory adopt()s the C result,
	// the graphics.hpp Device-factory idiom.
	//It does NOT own the bytes:
	// fromBufferRegion wraps a const ref over the caller's Buffer (Buffer_createRefFromBuffer),
	// so that Buffer must outlive every copy of the stream and must not move (resize/release) while one is alive,
	// the same contract MemoryStream_createFromBufferRegion documents.
	//Read-only on purpose:
	// a writable stream over borrowed bytes needs a mutable ref, and a consumer, first.

	class MemoryStream {

		RefPtr<c::MemoryStream> ref;

	public:

		MemoryStream() noexcept = default;

		//len == 0 streams to the end of the buffer. memory_stream.h documents exactly that convention,
		// but the implementation does not honour it:
		// createFromBufferRegion passes length straight to Buffer_setLength (MemoryStream_createFromBufferRegion),
		// so 0 yields an EMPTY stream and the first read fails with "stream out of bounds".
		//Resolved here, where the buffer is in hand; drop this once the C side implements its own documented behaviour.

		[[nodiscard]] static c::Bool fromBufferRegion(
			const Buffer &bytes, const MemoryStreamType &types, MemoryStream &result,
			c::U64 off = 0, c::U64 len = 0, c::Error *e_rr = nullptr
		) noexcept {

			result = MemoryStream();
			c::MemoryStreamRef *raw = nullptr;

			const c::U64 total = c::Buffer_length(bytes.handle());

			if(!c::MemoryStream_createFromBufferRegion(
				c::Buffer_createRefFromBuffer(bytes.handle(), true), off,
				len ? len : (total > off ? total - off : 0),
				c::EMemoryStreamFlags_None, &types.type, &raw, e_rr
			))
				return false;

			result.ref = RefPtr<c::MemoryStream>::adopt(raw);
			return true;
		}

		[[nodiscard]] c::Bool valid() const noexcept { return ref.valid(); }
		[[nodiscard]] explicit operator bool() const noexcept { return ref.valid(); }

		//MemoryStreamRef and the StreamRef the oiSH reader consumes are both bare RefPtr typedefs,
		// so one handle serves every C stream consumer without main.c's (StreamRef*) cast.

		[[nodiscard]] c::RefPtr *handle() const noexcept { return ref.handle(); }
		[[nodiscard]] c::MemoryStream *data() const noexcept { return ref.data(); }
		void release() noexcept { ref.release(); }
	};

}
