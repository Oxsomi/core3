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

		//A decode running on a job queue (HDR_readBegin), finished by end() or, discarding its errors, the destructor.
		//Both have to run on the thread that owns the queue.

		class PendingRead {

			c::HDRReadPending *pending = nullptr;

		public:

			PendingRead() noexcept = default;
			~PendingRead() noexcept { (void) end(); }

			PendingRead(const PendingRead&) = delete;
			PendingRead &operator=(const PendingRead&) = delete;

			//fileBytes and output are borrowed until end; output may be any writable memory (see HDR_readBegin).

			[[nodiscard]] c::Bool begin(
				const c::Buffer &fileBytes, c::Buffer output, c::HDRInfo &info, const c::Allocator *alloc,
				c::JobQueue *jobs, c::EHDRReadFlags flags = c::EHDRReadFlags_None, c::Error *e_rr = nullptr
			) noexcept {
				(void) end();
				return c::HDR_readBegin(fileBytes, flags, &info, output, jobs, alloc, &pending, e_rr);
			}

			[[nodiscard]] c::Bool end(c::Error *e_rr = nullptr) noexcept {
				return !pending || c::HDR_readEnd(&pending, nullptr, e_rr);
			}

			[[nodiscard]] c::Bool active() const noexcept { return pending != nullptr; }
		};

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

		//jobs decodes the scanlines on that queue straight into result (HDR_readBegin), from the thread that owns it;
		// NULL decodes serially.

		[[nodiscard]] inline c::Bool read(
			const c::Buffer &fileBytes, const file::Types &types, Buffer &result, c::HDRInfo &info,
			const c::Allocator *alloc, c::JobQueue *jobs, c::EHDRReadFlags flags = c::EHDRReadFlags_None,
			c::Error *e_rr = nullptr
		) noexcept {

			result.release();

			if(jobs) {

				c::HDRInfo head{};
				c::U64 dataOffset = 0;

				if(!readHeader(fileBytes, types, head, dataOffset, alloc, flags, e_rr))
					return false;

				const c::U64 rowBytes = (c::U64) head.w * 4 * (flags & c::EHDRReadFlags_KeepRGBE ? 1 : sizeof(c::F32));

				if(!result.createUninitializedBytes(rowBytes * head.h, e_rr))
					return false;

				PendingRead read;

				if(!read.begin(fileBytes, result.handle(), info, alloc, jobs, flags, e_rr) || !read.end(e_rr)) {
					result.release();
					return false;
				}

				return true;
			}

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

		[[nodiscard]] inline c::Bool read(
			const c::Buffer &fileBytes, const file::Types &types, Buffer &result, c::HDRInfo &info,
			const c::Allocator *alloc, c::EHDRReadFlags flags = c::EHDRReadFlags_None,
			c::Error *e_rr = nullptr
		) noexcept {
			return read(fileBytes, types, result, info, alloc, nullptr, flags, e_rr);
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
		//
		//exposure is the multiplier the pixels already carry, recorded as the EXPOSURE header (see HDR_writeExposed).

		[[nodiscard]] inline c::Bool write(
			const StringView &loc, const c::Buffer &pixels, c::U32 width, c::U32 height,
			const file::Types &types, const c::Allocator *alloc,
			c::EHDRWriteFlags flags = c::EHDRWriteFlags_None, c::F32 exposure = 1, c::Error *e_rr = nullptr
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
				&loc.handle(), 0, c::EFileOpenType_Write, true, &types.fileHandle, &streamType, &outRaw, e_rr
			))
				return false;

			const RefPtr<c::OxStream> out = RefPtr<c::OxStream>::adopt(outRaw);
			c::U64 off = 0;

			return c::HDR_writeExposed(out.handle(), &off, flags, width, height, exposure, alloc, in.handle(), 0, e_rr);

		}
	}
}
