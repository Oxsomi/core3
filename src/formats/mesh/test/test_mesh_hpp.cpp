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

//formats/mesh/test/test_mesh_hpp.cpp
//
//Type check for the C++ mesh layers, in the shape test_bmp_hpp.cpp established: including the headers is what
// compiles the inline bodies, and naming the overloads inside an unevaluated sizeof adds the caller's half
// without odr-using anything the platforms layer would have to be linked for.
//
//It is not registered in test_mesh_main.c on purpose: a compile time check has nothing to assert at runtime.

#include "formats/obj.hpp"
#include "formats/ply.hpp"

//NEVER defined, and they do not need to be: an unevaluated operand does not odr-use what it names.

const oxc::c::Buffer &meshTypeCheckBytes();
const oxc::file::Types &meshTypeCheckTypes();
const oxc::c::Allocator *meshTypeCheckAlloc();
oxc::c::Error *meshTypeCheckError();
oxc::Buffer &meshTypeCheckBuffer();
oxc::c::MeshInfo &meshTypeCheckInfo();
const oxc::c::MeshOutput &meshTypeCheckOutput();

//Never invoked.

extern "C" void Test_meshHppTypeCheck() {

	using namespace oxc;

	static_assert(
		sizeof(obj::read(
			meshTypeCheckBytes(), meshTypeCheckTypes(), meshTypeCheckOutput(), meshTypeCheckInfo(), meshTypeCheckAlloc()
		)) == sizeof(c::Bool),
		"obj::read into sinks must stay callable on its defaults alone and must return c::Bool"
	);

	static_assert(
		sizeof(obj::read(
			meshTypeCheckBytes(), meshTypeCheckTypes(), meshTypeCheckBuffer(), meshTypeCheckBuffer(),
			&meshTypeCheckBuffer(), nullptr, meshTypeCheckInfo(), meshTypeCheckAlloc(),
			c::EMeshReadFlags_ComputeNormals, meshTypeCheckError()
		)) == sizeof(c::Bool),
		"obj::read into Buffers must stay callable with every argument spelled out and must return c::Bool"
	);

	static_assert(
		sizeof(ply::read(
			meshTypeCheckBytes(), meshTypeCheckTypes(), meshTypeCheckOutput(), meshTypeCheckInfo(), meshTypeCheckAlloc()
		)) == sizeof(c::Bool),
		"ply::read into sinks must stay callable on its defaults alone and must return c::Bool"
	);

	static_assert(
		sizeof(ply::read(
			meshTypeCheckBytes(), meshTypeCheckTypes(), meshTypeCheckBuffer(), meshTypeCheckBuffer(),
			nullptr, &meshTypeCheckBuffer(), meshTypeCheckInfo(), meshTypeCheckAlloc()
		)) == sizeof(c::Bool),
		"ply::read into Buffers must stay callable and must return c::Bool"
	);
}
