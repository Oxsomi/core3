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

//formats/mesh.hpp
//
//The glue every mesh reader's C++ layer shares: the bytes a consumer already holds become a stream, and the
// records come back either through sinks the caller owns or as owned Buffers. obj.hpp and ply.hpp are each a
// few lines over this, which is what keeps the two from disagreeing about the shape of a read.

#pragma once

#include "platforms/file.hpp"
#include "types/container/ref_ptr.hpp"

namespace oxc {

	namespace c {
		#include "formats/mesh/mesh.h"
		#include "types/container/memory_stream.h"
	}

	namespace mesh {

		//Both C readers have this signature, which is what lets one wrapper serve either.

		using ReadFunc = c::Bool (*)(
			c::StreamRef*, c::U64*, c::EMeshReadFlags, c::MeshInfo*, const c::MeshOutput*, const c::Allocator*, c::Error*
		);

		//The caller's bytes behind a readable stream. BORROWED: the stream takes a ref over them, so they must
		// outlive the call.

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

		//Into sinks the caller owns, which is the shape the readers are built for: records leave in chunks as the
		// file yields them and nothing here holds the mesh. A consumer uploading straight to buffers wants this one.

		[[nodiscard]] inline c::Bool read(
			ReadFunc fn, const c::Buffer &fileBytes, const file::Types &types, const c::MeshOutput &output,
			c::MeshInfo &info, const c::Allocator *alloc,
			c::EMeshReadFlags flags = c::EMeshReadFlags_None, c::Error *e_rr = nullptr
		) noexcept {

			const RefPtr<c::OxStream> in = sourceStream(fileBytes, types, e_rr);

			if(!in.valid())
				return false;

			c::U64 off = 0;
			return fn(in.handle(), &off, flags, &info, &output, alloc, e_rr);
		}

		//Into owned Buffers, one per output, for the consumer that genuinely wants the whole mesh in memory.
		//
		//This gives up what the readers are built for, deliberately: a mesh small enough to hold whole is the
		// common case for a tool, and the sinks are what a consumer with a smaller form should use instead.
		//attributes and triangles may be null to skip those outputs, and the reader then never computes them.

		[[nodiscard]] inline c::Bool read(
			ReadFunc fn, const c::Buffer &fileBytes, const file::Types &types,
			Buffer &positions, Buffer &indices, Buffer *attributes, Buffer *triangles,
			c::MeshInfo &info, const c::Allocator *alloc,
			c::EMeshReadFlags flags = c::EMeshReadFlags_None, c::Error *e_rr = nullptr
		) noexcept {

			positions.release();
			indices.release();

			if(attributes)
				attributes->release();

			if(triangles)
				triangles->release();

			const RefPtr<c::OxStream> in = sourceStream(fileBytes, types, e_rr);

			if(!in.valid())
				return false;

			//Four resizable sinks, each moved out into its Buffer afterwards; MemoryStream_move CONSUMES the
			// reference, so ownership leaves the RefPtr rather than being released twice.

			RefPtr<c::OxStream> sinks[4];
			Buffer *targets[4] = { &positions, &indices, attributes, triangles };

			for(c::U8 i = 0; i < 4; ++i) {

				if(!targets[i])
					continue;

				c::RefPtr *raw = nullptr;

				if(!c::MemoryStream_create(
					0, c::EMemoryStreamFlags_WriteResize, &types.memStream.type, (c::MemoryStreamRef**) &raw, e_rr
				))
					return false;

				sinks[i] = RefPtr<c::OxStream>::adopt(raw);
			}

			c::MeshOutput output{};
			output.positions = sinks[0].handle();
			output.indices = sinks[1].handle();
			output.attributes = attributes ? sinks[2].handle() : nullptr;
			output.triangles = triangles ? sinks[3].handle() : nullptr;

			c::U64 off = 0;

			if(!fn(in.handle(), &off, flags, &info, &output, alloc, e_rr))
				return false;

			for(c::U8 i = 0; i < 4; ++i) {

				if(!targets[i])
					continue;

				c::MemoryStreamRef *moved = (c::MemoryStreamRef*) sinks[i].steal();

				if(!c::MemoryStream_move(&moved, &targets[i]->handle(), e_rr))
					return false;
			}

			return true;
		}
	}
}
