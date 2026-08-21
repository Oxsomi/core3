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

//platforms/file.hpp
//
//C++ layer over the platforms file API.
//Everything platforms/file.h exposes is here:
// whole-file read/write, handles, streams, directory queries and virtual sections.
//Deliberately complete rather than demand-driven,
// a partial wrapper pushes the next consumer either into extending it or into reaching around it to the C API,
// and the second habit is the one that makes a wrapper pointless.
//
//NOT here, because they are not this layer:
// MemoryStream lives in types/container/memory_stream.hpp and SHFile in formats/oiSH.hpp.

#pragma once

#include "types/container/ref_ptr.hpp"
#include "types/container/string.hpp"
#include "types/container/buffer.hpp"
#include "types/container/memory_stream.hpp"

//Pre-include system headers used by the C headers below at global scope;
//they must not be pulled in for the first time inside a namespace.
//platform.h reaches lock.h and so atomic.h, which includes <atomic> on every non-Windows target, and a
//<atomic> first seen inside oxc::c declares operator new in a namespace, which is ill-formed.
//It happens to work today only because something else includes it at global scope first, which is an
//include order this header should not depend on (lock.hpp documents the same discipline).

#include <atomic>
#include <stdalign.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

namespace oxc {

	namespace c {
		#include "platforms/platform.h"
		#include "platforms/file.h"
	}

	namespace file {

		//The RefPtrTypes file objects are created through, and the allocator baked into them.
		//Must outlive every handle, stream and read made through it:
		// File_read allocates its output from this allocator, so a Buffer receiving it has to be built on the same one.

		struct Types {

			//Composed, not re-declared:
			// a virtual section is mounted as a memory stream,
			// so the file API needs the same RefPtrType the stream wrapper does, and one owner avoids two.

			MemoryStreamType memStream;

			c::RefPtrType fileHandle;
			c::RefPtrType fileStream;

			explicit Types(const c::Allocator *alloc) noexcept :
				memStream(alloc),
				fileHandle(c::FileHandle_makeType(alloc)),
				fileStream(c::FileStream_makeType(alloc)) {}
		};

		//Mounts a //section so the reads below resolve.
		//Encrypted sections stay on the C API until a consumer brings a key.

		[[nodiscard]] inline c::Bool loadVirtual(
			const StringView &loc, const Types &types,
			const c::Allocator *alloc, c::Error *e_rr = nullptr
		) noexcept {
			return c::File_loadVirtual(&loc.handle(), &types.memStream.type, nullptr, nullptr, alloc, e_rr);
		}

		[[nodiscard]] inline c::Bool unloadVirtual(
			const StringView &loc, const c::Allocator *alloc, c::Error *e_rr = nullptr
		) noexcept {
			return c::File_unloadVirtual(&loc.handle(), alloc, e_rr);
		}

		//No e_rr:
		// the C File_has swallows the lookup error by design, absence IS the answer.

		[[nodiscard]] inline c::Bool has(const StringView &loc, const c::Allocator *alloc) noexcept {
			return c::File_has(&loc.handle(), alloc);
		}

		//Whole file (or [off, off+len)) into an owned Buffer; len == 0 means the remainder.
		//CAVEAT, and it bites exactly the paths this header exists for:
		// on a VIRTUAL location ("//section/...") File_read hands off to File_readVirtual (File_readVirtual),
		// which takes neither off nor len, the whole entry comes back and both arguments are silently ignored.
		//Slice the returned Buffer yourself for virtual reads. out is reset first,
		// File_read refuses a filled output (it would indicate a leak),
		// and MUST be built on the allocator types was made with (see FileTypes).

		[[nodiscard]] inline c::Bool read(
			const StringView &loc, const Types &types, Buffer &out,
			c::U64 off = 0, c::U64 len = 0, c::Ns maxTimeout = c::U64_MAX,
			c::Error *e_rr = nullptr
		) noexcept {
			out.release();
			return c::File_read(&loc.handle(), maxTimeout, off, len, &types.fileHandle, &out.handle(), e_rr);
		}

		//Borrowed-bytes overload:
		// a readback callback hands out a c::Buffer it still owns, and the owning oxc::Buffer would try to free it.
		//Same call, no ownership implied.

		[[nodiscard]] inline c::Bool write(
			const c::Buffer &data, const StringView &loc, const Types &types,
			c::U64 off = 0, c::U64 len = 0, c::Ns maxTimeout = c::U64_MAX,
			c::Bool createParent = true, c::Error *e_rr = nullptr
		) noexcept {
			return c::File_write(&data, &loc.handle(), off, len, maxTimeout, createParent, &types.fileHandle, e_rr);
		}

		[[nodiscard]] inline c::Bool write(
			const Buffer &data, const StringView &loc, const Types &types,
			c::U64 off = 0, c::U64 len = 0, c::Ns maxTimeout = c::U64_MAX,
			c::Bool createParent = true, c::Error *e_rr = nullptr
		) noexcept {
			return c::File_write(&data.handle(), &loc.handle(), off, len, maxTimeout, createParent, &types.fileHandle, e_rr);
		}

		//------------------------------------------------ existence, metadata, directory queries

		[[nodiscard]] inline c::Bool getInfo(
			const StringView &loc, c::FileInfo &info, const c::Allocator *alloc, c::Error *e_rr = nullptr
		) noexcept {
			return c::File_getInfo(&loc.handle(), &info, alloc, e_rr);
		}

		[[nodiscard]] inline c::Bool isVirtualLoaded(
			const StringView &loc, const c::Allocator *alloc, c::Error *e_rr = nullptr
		) noexcept {
			return c::File_isVirtualLoaded(&loc.handle(), alloc, e_rr);
		}

		//Every callback path arrives fully qualified, absolute for physical entries and //-prefixed for virtual ones,
		// and either form feeds straight back into the other file::
		// calls.

		[[nodiscard]] inline c::Bool foreach(
			const StringView &loc, c::FileCallback callback, void *userData, c::Bool isRecursive,
			const c::Allocator *alloc, c::Error *e_rr = nullptr, c::Bool inAppDir = false
		) noexcept {
			return c::File_foreach(&loc.handle(), inAppDir, callback, userData, isRecursive, alloc, e_rr);
		}

		//Files only; countAll includes folders.

		[[nodiscard]] inline c::Bool count(
			const StringView &loc, c::EFileType type, c::Bool isRecursive, c::U64 &result,
			const c::Allocator *alloc, c::Error *e_rr = nullptr
		) noexcept {
			return c::File_queryFileObjectCount(&loc.handle(), type, isRecursive, &result, alloc, e_rr);
		}

		[[nodiscard]] inline c::Bool countAll(
			const StringView &loc, c::Bool isRecursive, c::U64 &result,
			const c::Allocator *alloc, c::Error *e_rr = nullptr
		) noexcept {
			return c::File_queryFileObjectCountAll(&loc.handle(), isRecursive, &result, alloc, e_rr);
		}

		//------------------------------------------------ mutating the filesystem

		[[nodiscard]] inline c::Bool add(
			const StringView &loc, c::EFileType type, const c::Allocator *alloc,
			c::Error *e_rr = nullptr, c::Bool createParentOnly = false
		) noexcept {
			return c::File_add(&loc.handle(), type, createParentOnly, alloc, e_rr);
		}

		[[nodiscard]] inline c::Bool remove(
			const StringView &loc, const c::Allocator *alloc,
			c::Error *e_rr = nullptr, c::Ns maxTimeout = c::U64_MAX
		) noexcept {
			return c::File_remove(&loc.handle(), maxTimeout, alloc, e_rr);
		}

		//newFileName is a NAME, not a path; move() takes the destination directory.

		[[nodiscard]] inline c::Bool rename(
			const StringView &loc, const StringView &newFileName, const c::Allocator *alloc,
			c::Error *e_rr = nullptr, c::Ns maxTimeout = c::U64_MAX
		) noexcept {
			return c::File_rename(&loc.handle(), &newFileName.handle(), maxTimeout, alloc, e_rr);
		}

		[[nodiscard]] inline c::Bool move(
			const StringView &loc, const StringView &directoryName, const c::Allocator *alloc,
			c::Error *e_rr = nullptr, c::Ns maxTimeout = c::U64_MAX
		) noexcept {
			return c::File_move(&loc.handle(), &directoryName.handle(), maxTimeout, alloc, e_rr);
		}
	}

	//---------------------------------------------------------------- handles

	//Owning handle to an open file, for the reads and writes that shouldn't re-open per call.
	//Copyable like the other RefPtr wrappers; the file closes when the last reference goes.

	class FileHandle {

		RefPtr<c::FileHandle> ref;

	public:

		FileHandle() noexcept = default;

		[[nodiscard]] static c::Bool open(
			const StringView &loc, const file::Types &types, FileHandle &result,
			c::EFileOpenType type = c::EFileOpenType_Read, c::Bool create = false,
			c::Error *e_rr = nullptr, c::Ns timeout = c::U64_MAX
		) noexcept {

			result = FileHandle();
			c::FileHandleRef *raw = nullptr;

			if(!c::File_open(&loc.handle(), timeout, type, create, &types.fileHandle, &raw, e_rr))
				return false;

			result.ref = RefPtr<c::FileHandle>::adopt(raw);
			return true;
		}

		//len 0 reads to the end. out is reset first,
		// the C side refuses a filled output because that would indicate a leak,
		// and must be built on the allocator Types was made with.

		[[nodiscard]] c::Bool read(
			Buffer &out, c::U64 off = 0, c::U64 len = 0, c::Error *e_rr = nullptr
		) const noexcept {
			out.release();
			return c::FileHandleRef_read(ref.handle(), off, len, &out.handle(), e_rr);
		}

		[[nodiscard]] c::Bool write(
			const c::Buffer &data, c::U64 off = 0, c::U64 len = 0, c::Error *e_rr = nullptr
		) noexcept {
			return c::FileHandleRef_write(ref.handle(), off, len, &data, e_rr);
		}

		[[nodiscard]] c::Bool valid() const noexcept { return ref.valid(); }
		[[nodiscard]] explicit operator bool() const noexcept { return ref.valid(); }
		[[nodiscard]] c::RefPtr *handle() const noexcept { return ref.handle(); }
		[[nodiscard]] c::FileHandle *data() const noexcept { return ref.data(); }
		void release() noexcept { ref.release(); }
	};

	//---------------------------------------------------------------- streams

	//A seekable stream over a file, for inputs too big to land in one Buffer.
	//Crosses to any StreamRef consumer unchanged, which is the same shape MemoryStream has,
	// so an oiSH can be read from either without the reader knowing which.

	class FileStream {

		RefPtr<c::FileStream> ref;

	public:

		FileStream() noexcept = default;

		[[nodiscard]] static c::Bool open(
			const StringView &loc, const file::Types &types, FileStream &result,
			c::EFileOpenType type = c::EFileOpenType_Read, c::Bool create = false,
			c::Error *e_rr = nullptr, c::Ns timeout = c::U64_MAX
		) noexcept {

			result = FileStream();
			c::StreamRef *raw = nullptr;

			if(!c::File_openStream(
				&loc.handle(), timeout, type, create, &types.fileHandle, &types.fileStream, &raw, e_rr
			))
				return false;

			result.ref = RefPtr<c::FileStream>::adopt(raw);
			return true;
		}

		//TAKES OVER the handle:
		// it is released here whether or not the stream opens,
		// so the caller's FileHandle is empty afterwards either way.

		[[nodiscard]] static c::Bool adopt(
			FileHandle &handle, const file::Types &types, FileStream &result, c::Error *e_rr = nullptr
		) noexcept {

			result = FileStream();

			c::FileHandleRef *h = handle.handle();
			c::StreamRef *raw = nullptr;

			const c::Bool ok = c::FileHandle_openStream(&h, &types.fileStream, &raw, e_rr);

			handle.release();

			if(!ok)
				return false;

			result.ref = RefPtr<c::FileStream>::adopt(raw);
			return true;
		}

		[[nodiscard]] c::Bool valid() const noexcept { return ref.valid(); }
		[[nodiscard]] explicit operator bool() const noexcept { return ref.valid(); }
		[[nodiscard]] c::RefPtr *handle() const noexcept { return ref.handle(); }
		[[nodiscard]] c::FileStream *data() const noexcept { return ref.data(); }
		void release() noexcept { ref.release(); }
	};
}
