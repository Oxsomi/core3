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

//formats/ply.hpp
//
//C++ layer over the PLY reader. Everything but the name of the C entry point lives in mesh.hpp, since a mesh is
// a mesh whichever file it came from.

#pragma once

#include "types/mesh.hpp"

namespace oxc {

	namespace c {
		#include "formats/ply/ply_file.h"
	}

	namespace ply {

		//Into sinks the caller owns. See mesh::read.
		//
		//jobs is lent to Ply_readParallel for exactly this call and touches nothing once it returns; NULL (the
		//default) is Ply_read. A caller holding an oxc::JobQueue passes jobs.handle(). Splitting is still
		// Ply_readParallel's own call to make: a stream that cannot declare EStreamType_ConcurrentRead, an ascii
		// file or one of the few binary shapes it does not recognize all run Ply_read regardless of jobs.

		[[nodiscard]] inline c::Bool read(
			const c::Buffer &fileBytes, const file::Types &types, const c::MeshOutput &output,
			c::MeshInfo &info, const c::Allocator *alloc,
			c::EMeshFlags flags = c::EMeshFlags_None, c::JobQueue *jobs = nullptr, c::Error *e_rr = nullptr
		) noexcept {

			const RefPtr<c::OxStream> in = mesh::sourceStream(fileBytes, types, e_rr);

			if(!in.valid())
				return false;

			c::U64 off = 0;
			return c::Ply_readParallel(in.handle(), &off, flags, &info, &output, jobs, false, alloc, e_rr);
		}

		//Into owned Buffers, which materializes the whole mesh. See mesh::read for when that is the wrong call.
		//jobs is as above.

		[[nodiscard]] inline c::Bool read(
			const c::Buffer &fileBytes, const file::Types &types,
			Buffer &positions, Buffer &indices, Buffer *attributes, Buffer *triangles,
			c::MeshInfo &info, const c::Allocator *alloc,
			c::EMeshFlags flags = c::EMeshFlags_None, c::JobQueue *jobs = nullptr, c::Error *e_rr = nullptr
		) noexcept {

			positions.release();
			indices.release();

			if(attributes)
				attributes->release();

			if(triangles)
				triangles->release();

			const RefPtr<c::OxStream> in = mesh::sourceStream(fileBytes, types, e_rr);

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

			if(!c::Ply_readParallel(in.handle(), &off, flags, &info, &output, jobs, false, alloc, e_rr))
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
