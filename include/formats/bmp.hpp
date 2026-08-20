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

//formats/bmp.hpp
//
//C++ layer over the oiBMP reader/writer.
//Small on purpose:
// BMP_write is stream-to-stream,
// and the two streams a consumer actually has are "pixels I already hold" and "a path to write",
// so the wrapper is the glue that turns those into streams, not a re-modelling of the format.

#pragma once

#include "platforms/file.hpp"

namespace oxc {

	namespace c {
		//ETextureFormatId, which BMPInfo carries.
		//Arrived transitively through the file header until that stopped pulling the oiSH chain,
		// so it is named here rather than relied on.
		#include "types/container/texture_format.h"
		#include "formats/bmp/bmp_file.h"
	}

	namespace bmp {

		//Writes pixels straight to a file.
		//Rows are top-down in memory by default, which is what a GPU readback hands back, and BMP stores bottom-up,
		// the flip is the writer's job, so nothing here reorders rows by hand.
		//
		//discardAlpha selects the INPUT layout, not just the output:
		// false (the default) means the caller supplies BGRA8,
		// true means the caller already supplies BGR8 (BMP_write's own note:
		// "a stream that needs to match the input's format").
		//Handing BGRA8 to the true form reads 3-byte pixels out of 4-byte data and smears the image into vertical
		// bands.
		//
		//pixels must stay alive for the call.
		//It crosses as a c::Buffer rather than oxc::Buffer because the usual source is a readback callback's borrowed
		// buffer, which this must not own.

		[[nodiscard]] inline c::Bool write(
			const StringView &loc, const c::Buffer &pixels, c::U32 width, c::U32 height,
			const file::Types &types, const c::Allocator *alloc,
			c::Bool discardAlpha = false, c::Bool inputIsFlipped = false, c::Error *e_rr = nullptr
		) noexcept {

			c::BMPInfo info{};
			info.w = width;
			info.h = height;
			info.textureFormatId = (c::U8) c::ETextureFormatId_BGRA8;
			info.discardAlpha = discardAlpha;

			const c::RefPtrType streamType = c::FileStream_makeType(alloc);

			c::StreamRef *in = nullptr, *out = nullptr;
			c::Bool s_uccess = false;

			//A REF of the caller's bytes:
			// createFromBuffer takes ownership of an allocation,
			// and these pixels are borrowed (a readback callback still owns them).

			c::Buffer ref = c::Buffer_createRefFromBuffer(pixels, true);

			if(c::MemoryStream_createFromBuffer(
				&ref, c::EMemoryStreamFlags_None, &types.memStream.type, (c::MemoryStreamRef**) &in, e_rr
			)) {

				if(c::File_openStream(
					&loc.handle(), c::U64_MAX, c::EFileOpenType_Write, true,
					&types.fileHandle, &streamType, &out, e_rr
				)) {
					c::U64 off = 0;
					s_uccess = c::BMP_write(out, &off, &info, alloc, in, 0, inputIsFlipped, e_rr);
				}
			}

			c::RefPtr_dec(&out);
			c::RefPtr_dec(&in);
			return s_uccess;
		}
	}
}
