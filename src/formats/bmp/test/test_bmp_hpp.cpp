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

//formats/bmp/test/test_bmp_hpp.cpp
//
//Type check for the C++ oiBMP layer.
//formats/bmp.hpp is hand written and, until this file existed, no translation unit included it,
// so nothing had ever compiled it.
//This TU exists so the header is built by every configuration the tests are,
// which is what stops it drifting from the C API it wraps.
//
//The body below is deliberately never CALLED.
//Simply including the header is what compiles bmp::write:
// a non-template function definition is typechecked wherever it appears, used or not.
//Naming it below adds the caller's half of that, overload resolution, the implicit StringView conversion and
// both trailing default arguments.
//
//The naming sits inside static_assert(sizeof(...)), an UNEVALUATED operand, and that is not an accident.
//An evaluated call would odr-use bmp::write and emit its inline body, which reaches File_openStream and
// FileStream_makeType.
//Both live in OxC3_platforms, and OxC3_formats_bmp_test links OxC3_types_container_test_util and
// OxC3_formats_bmp only, so the link would fail on those two symbols.
//Constructing a file::Types instead of borrowing one hits the same wall,
// its constructor calls FileHandle_makeType and FileStream_makeType.
//Dragging the whole platforms layer into a format suite costs more than that buys,
// the compile coverage is identical either way.
//
//It is not registered in test_bmp_main.c on purpose:
// a compile time check has nothing to assert at runtime.

#include "formats/bmp.hpp"

//NEVER defined, and they do not need to be.
//An unevaluated operand does not odr-use what it names, so the linker is never asked for any of these,
// which is the whole point:
// a file::Types can only be built by the platforms layer this suite does not link.

const oxc::c::Buffer &bmpTypeCheckPixels();
const oxc::file::Types &bmpTypeCheckTypes();
const oxc::c::Allocator *bmpTypeCheckAlloc();
oxc::c::Error *bmpTypeCheckError();

//Never invoked.
//See the file comment: this is a compile time check, not a test module.

extern "C" void Test_bmpHppTypeCheck() {

	using namespace oxc;

	//The short form, leaning on all three default arguments.
	//The literal exercises the implicit const C8* to StringView conversion the wrapper relies on.

	static_assert(
		sizeof(bmp::write(
			"out.bmp", bmpTypeCheckPixels(), 64, 64, bmpTypeCheckTypes(), bmpTypeCheckAlloc()
		)) == sizeof(c::Bool),
		"bmp::write must stay callable on its defaults alone and must return c::Bool"
	);

	//And the long form, every argument spelled out.

	static_assert(
		sizeof(bmp::write(
			"out.bmp", bmpTypeCheckPixels(), 64, 64, bmpTypeCheckTypes(), bmpTypeCheckAlloc(),
			true, true, bmpTypeCheckError()
		)) == sizeof(c::Bool),
		"bmp::write must stay callable with every argument spelled out and must return c::Bool"
	);
}
