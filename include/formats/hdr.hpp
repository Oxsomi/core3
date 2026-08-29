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

//formats/hdr.hpp
//
//C++ layer over the Radiance HDR reader/writer. Small on purpose, for the same reason bmp.hpp is:
//both entry points are stream-to-stream, and the streams a consumer actually has are "bytes I already hold" and "a path",
// so the wrapper is the glue that turns those into streams, not a re-modelling of the format.

#pragma once

#include "platforms/file.hpp"
#include "types/container/ref_ptr.hpp"

namespace oxc {

	namespace c {
		#include "formats/hdr/hdr_file.h"
	}

	namespace hdr {

		//Every entry point starts the same way, with the caller's bytes behind a readable stream.
		//The bytes are BORROWED: the stream takes a ref over them, so they must outlive the call.

		[[nodiscard]] inline RefPtr<c::OxStream> sourceStream(
			const c::Buffer &fileBytes, const file::Types &types, c::Error *e_rr
		) noexcept {

			c::RefPtr *raw = nullptr;

			if(!c::MemoryStream_createFromBufferRegion(
				c::Buffer_createRefFromBuffer(fileBytes, true), 0, c::Buffer_length(fileBytes),
				c::EMemoryStreamFlags_None, &types.memStream.type, (c::MemoryStreamRef**) &raw, e_rr
			))
				return RefPtr<c::OxStream>();

			return RefPtr<c::OxStream>::adopt(raw);
		}

		//The header alone, and where the pixel data begins. Nothing is decoded, so this costs a few hundred bytes of
		// reading however large the file is.

		[[nodiscard]] inline c::Bool readHeader(
			const c::Buffer &fileBytes, const file::Types &types, c::HDRInfo &info, c::U64 &dataOffset,
			const c::Allocator *alloc, c::EHDRReadFlags flags = c::EHDRReadFlags_None, c::Error *e_rr = nullptr
		) noexcept {

			const RefPtr<c::OxStream> in = sourceStream(fileBytes, types, e_rr);

			if(!in.valid())
				return false;

			dataOffset = 0;
			return c::HDR_readHeader(in.handle(), &dataOffset, flags, &info, alloc, e_rr);
		}

		//Decodes into a sink the caller owns, which is the shape the decoder is actually built for: rows arrive one at
		// a time and nothing here holds the image. A consumer uploading bands to the GPU, or re-compressing them, wants
		// this one rather than the allocating overload below.

		[[nodiscard]] inline c::Bool read(
			const c::Buffer &fileBytes, const file::Types &types, c::StreamRef *sink, c::HDRInfo &info,
			const c::Allocator *alloc, c::U64 sinkOffset = 0,
			c::EHDRReadFlags flags = c::EHDRReadFlags_None, c::Error *e_rr = nullptr
		) noexcept {

			const RefPtr<c::OxStream> in = sourceStream(fileBytes, types, e_rr);

			if(!in.valid())
				return false;

			c::U64 off = 0;
			return c::HDR_read(in.handle(), &off, flags, &info, sink, sinkOffset, alloc, e_rr);
		}

		//Decodes a Radiance file already held in memory, into ONE allocation.
		//
		//This deliberately gives up what HDR_read is built for. The decoder writes a scanline at a time and holds
		// nothing, so a consumer that can take the image in bands should call it directly with a sink of its own
		// and never allocate the whole thing. This wrapper is for the consumers that genuinely want it whole,
		// where the convenience is worth the allocation and there is no smaller form to work in.
		//
		//result is w * h * 4 with row 0 at the TOP: F32s, or the RGBE plane under KeepRGBE, which is a quarter
		// the size and what a consumer that decodes on the GPU wants. Owned by the caller, and released first the
		// way File_read does.
		//
		//The source bytes are BORROWED: the stream takes a ref over them, so they must outlive the call and are
		// never freed by it.

		[[nodiscard]] inline c::Bool read(
			const c::Buffer &fileBytes, const file::Types &types, Buffer &result, c::HDRInfo &info,
			const c::Allocator *alloc, c::EHDRReadFlags flags = c::EHDRReadFlags_None,
			c::Error *e_rr = nullptr
		) noexcept {

			result.release();

			const RefPtr<c::OxStream> in = sourceStream(fileBytes, types, e_rr);

			if(!in.valid())
				return false;

			c::RefPtr *sinkRaw = nullptr;

			if(!c::MemoryStream_create(
				0, c::EMemoryStreamFlags_WriteResize, &types.memStream.type, (c::MemoryStreamRef**) &sinkRaw, e_rr
			))
				return false;

			RefPtr<c::OxStream> sink = RefPtr<c::OxStream>::adopt(sinkRaw);
			c::U64 off = 0;

			if(!c::HDR_read(in.handle(), &off, flags, &info, sink.handle(), 0, alloc, e_rr))
				return false;

			//MemoryStream_move CONSUMES the reference, so ownership leaves the RefPtr rather than being released twice.

			c::MemoryStreamRef *moved = (c::MemoryStreamRef*) sink.steal();
			return c::MemoryStream_move(&moved, &result.handle(), e_rr);

		}

		//Writes linear radiance straight to a file.
		//
		//pixels is w * h * 4 with row 0 at the TOP: F32s by default, or the RGBE plane itself under SourceIsRGBE,
		// in which case nothing re-encodes it. Top-down is both what a GPU readback hands back and what Radiance's -Y +X header
		// declares, so nothing here reorders rows. Alpha is read but dropped: the format carries three channels over a shared
		// exponent.
		//
		//The buffer is BORROWED, since a readback callback still owns it, so the memory stream takes a ref rather than the
		// allocation the way createFromBuffer otherwise would.

		[[nodiscard]] inline c::Bool write(
			const StringView &loc, const c::Buffer &pixels, c::U32 width, c::U32 height,
			const file::Types &types, const c::Allocator *alloc,
			c::EHDRWriteFlags flags = c::EHDRWriteFlags_None, c::Error *e_rr = nullptr
		) noexcept {

			const c::RefPtrType streamType = c::FileStream_makeType(alloc);

			//A ref of the caller's bytes: createFromBuffer takes ownership of an allocation, and these are borrowed,
			// since a readback callback still owns them.

			c::Buffer ref = c::Buffer_createRefFromBuffer(pixels, true);
			c::RefPtr *inRaw = nullptr, *outRaw = nullptr;

			if(!c::MemoryStream_createFromBuffer(
				&ref, c::EMemoryStreamFlags_None, &types.memStream.type, (c::MemoryStreamRef**) &inRaw, e_rr
			))
				return false;

			const RefPtr<c::OxStream> in = RefPtr<c::OxStream>::adopt(inRaw);

			if(!c::File_openStream(
				&loc.handle(), c::U64_MAX, c::EFileOpenType_Write, true, &types.fileHandle, &streamType, &outRaw, e_rr
			))
				return false;

			const RefPtr<c::OxStream> out = RefPtr<c::OxStream>::adopt(outRaw);
			c::U64 off = 0;

			return c::HDR_write(out.handle(), &off, flags, width, height, alloc, in.handle(), 0, e_rr);

		}
	}
}
