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

//types/container/ref_ptr.hpp

#pragma once
#include <utility>

//Pre-include system headers used by the C headers below at global scope;
//they must not be pulled in for the first time inside a namespace.

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <atomic>

//RAII wrapper around the C RefPtr.
//The refcount, the allocation and the free callback all stay in the C side; this only automates the
//inc/dec pairs, which is where the leaks and the use-after-frees actually come from.
//
//  oxc::RefPtr<c::MemoryStream> stream;
//  if(!oxc::RefPtr<c::MemoryStream>::create(&memStreamType, stream, e_rr))
//      ...
//  stream->parent.size;              //-> operator forwards to RefPtr_data
//  c::CAFile_read(stream.handle(), ...);
//
//Unlike the other wrappers this one is copyable: copying is what a refcount is for. A copy increments,
// a move doesn't, and both leave the source valid (empty after a move).
//
//The C API's WeakRefPtr is the same type with a promise attached
// (the owner outlives the borrower, so no inc/dec is needed).
//That promise can't be expressed here, so borrow with handle() rather than wrapping.

namespace oxc {

	namespace c {
		#include "types/container/ref_ptr.h"
		#include "types/base/error.h"
		#include "types/base/allocator.h"
	}

	template<typename T>
	class RefPtr {

		c::RefPtr *ptr;

	public:

		RefPtr() noexcept : ptr(nullptr) {}
		~RefPtr() noexcept { release(); }

		RefPtr(const RefPtr &other) noexcept : ptr(other.ptr) {
			if(ptr)
				c::RefPtr_inc(ptr);
		}

		RefPtr &operator=(const RefPtr &other) noexcept {

			if(this == &other)
				return *this;

			//inc before release, so self-assignment through an alias can't drop the last reference
			if(other.ptr)
				c::RefPtr_inc(other.ptr);

			release();
			ptr = other.ptr;
			return *this;
		}

		RefPtr(RefPtr &&other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }

		RefPtr &operator=(RefPtr &&other) noexcept {

			if(this == &other)
				return *this;

			release();
			ptr = other.ptr;
			other.ptr = nullptr;
			return *this;
		}

		//Allocates a new object of `type`. The type has to outlive every RefPtr made from it (see ref_ptr.h).
		[[nodiscard]] static c::Bool create(const c::RefPtrType *type, RefPtr &result, c::Error *e_rr = nullptr) noexcept {
			result.release();
			return c::RefPtr_create(type, &result.ptr, e_rr);
		}

		//Takes over a raw RefPtr* without incrementing; use for a pointer a C function just handed you.
		[[nodiscard]] static RefPtr adopt(c::RefPtr *raw) noexcept {
			RefPtr out;
			out.ptr = raw;
			return out;
		}

		//Adds a reference to a RefPtr* someone else owns.
		[[nodiscard]] static RefPtr share(c::RefPtr *raw) noexcept {
			RefPtr out;
			if(raw && c::RefPtr_inc(raw))
				out.ptr = raw;
			return out;
		}

		void release() noexcept {
			if(ptr)
				c::RefPtr_dec(&ptr);
			ptr = nullptr;
		}

		//Access. Mirrors the RefPtr_data macro: the payload sits directly behind the header.

		[[nodiscard]] T *data() const noexcept { return ptr ? (T*)(ptr + 1) : nullptr; }
		[[nodiscard]] T *operator->() const noexcept { return data(); }
		[[nodiscard]] T &operator*() const noexcept { return *data(); }

		[[nodiscard]] explicit operator bool() const noexcept { return ptr != nullptr; }
		[[nodiscard]] bool valid() const noexcept { return ptr != nullptr; }

		[[nodiscard]] c::TypeId typeId() const noexcept { return ptr ? ptr->refPtrType->typeId : (c::TypeId) 0; }

		[[nodiscard]] bool operator==(const RefPtr &o) const noexcept { return ptr == o.ptr; }
		[[nodiscard]] bool operator!=(const RefPtr &o) const noexcept { return ptr != o.ptr; }

		//Interop with C: borrow or transfer ownership of the raw RefPtr

		[[nodiscard]] c::RefPtr *handle() const noexcept { return ptr; }

		//Caller takes over the reference this held; must RefPtr_dec it
		[[nodiscard]] c::RefPtr *steal() noexcept {
			c::RefPtr *tmp = ptr;
			ptr = nullptr;
			return tmp;
		}
	};
}
