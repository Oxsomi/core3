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

//formats/obj.hpp
//
//C++ layer over the OBJ reader. Everything but the name of the C entry point lives in mesh.hpp, since a mesh is
// a mesh whichever file it came from.

#pragma once

#include "formats/mesh.hpp"

namespace oxc {

	namespace c {
		#include "formats/obj/obj_file.h"
	}

	namespace obj {

		//Into sinks the caller owns. See mesh::read.

		[[nodiscard]] inline c::Bool read(
			const c::Buffer &fileBytes, const file::Types &types, const c::MeshOutput &output,
			c::MeshInfo &info, const c::Allocator *alloc,
			c::EMeshReadFlags flags = c::EMeshReadFlags_None, c::Error *e_rr = nullptr
		) noexcept {
			return mesh::read(c::OBJ_read, fileBytes, types, output, info, alloc, flags, e_rr);
		}

		//Into owned Buffers, which materializes the whole mesh. See mesh::read for when that is the wrong call.

		[[nodiscard]] inline c::Bool read(
			const c::Buffer &fileBytes, const file::Types &types,
			Buffer &positions, Buffer &indices, Buffer *attributes, Buffer *triangles,
			c::MeshInfo &info, const c::Allocator *alloc,
			c::EMeshReadFlags flags = c::EMeshReadFlags_None, c::Error *e_rr = nullptr
		) noexcept {
			return mesh::read(
				c::OBJ_read, fileBytes, types, positions, indices, attributes, triangles, info, alloc, flags, e_rr
			);
		}
	}
}
