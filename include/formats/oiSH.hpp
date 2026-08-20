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

//formats/oiSH.hpp
//
//C++ layer over the oiSH reader, mirroring formats/bmp.hpp:
// a format wrapper belongs with its format, not with whatever subsystem first read one.
//Reading needs a StreamRef, which is why this pulls the memory-stream wrapper rather than the file API,
// a shader binary can come from anywhere that can produce a stream.

#pragma once

#include "types/container/memory_stream.hpp"

namespace oxc {

	namespace c {
		#include "formats/oiSH/sh_file.h"
	}

	//Owner over c::SHFile, the generator's home shape (a struct, an allocator and a free),
	// by hand only because its reader takes a StreamRef.
	//Move-only like oxc::Buffer:
	// a copy would need an allocator decision the caller should make explicitly (SHFile_combine, some day).

	class SHFile {

		c::SHFile self;
		const c::Allocator *alloc;

	public:

		SHFile(const c::Allocator &alloc) noexcept : self{}, alloc(&alloc) {}
		~SHFile() noexcept { release(); }

		SHFile(const SHFile&) = delete;
		SHFile &operator=(const SHFile&) = delete;

		SHFile(SHFile &&other) noexcept : self(other.self), alloc(other.alloc) {
			other.self = c::SHFile{};
		}

		SHFile &operator=(SHFile &&other) noexcept {

			if(this == &other)
				return *this;

			release();
			self = other.self;
			alloc = other.alloc;
			other.self = c::SHFile{};
			return *this;
		}

		void release() noexcept {
			c::SHFile_free(&self, alloc);
			self = c::SHFile{};
		}

		//Resets *this first,
		// so an SHFile can be refilled (shader hot reload) without leaking. offset advances past the oiSH,
		// leaving the stream usable for whatever follows it.

		[[nodiscard]] c::Bool read(
			const MemoryStream &stream, c::U64 *offset, c::Bool isSubFile = false, c::Error *e_rr = nullptr
		) noexcept {
			release();
			return c::SHFile_read(stream.handle(), offset, isSubFile, alloc, &self, e_rr);
		}

		//Crosses to C consumers unchanged (renderer bridges take an SHFile*).

		[[nodiscard]] c::SHFile &handle() noexcept { return self; }
		[[nodiscard]] const c::SHFile &handle() const noexcept { return self; }
	};
}
